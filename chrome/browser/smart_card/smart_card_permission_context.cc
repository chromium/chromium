// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/smart_card/smart_card_permission_context.h"

#include <algorithm>
#include <vector>

#include "base/containers/to_vector.h"
#include "base/power_monitor/power_monitor.h"
#include "base/power_monitor/power_observer.h"
#include "base/scoped_observation.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/permissions/one_time_permissions_tracker.h"
#include "chrome/browser/permissions/one_time_permissions_tracker_factory.h"
#include "chrome/browser/permissions/one_time_permissions_tracker_observer.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/smart_card/smart_card_reader_tracker.h"
#include "chrome/browser/smart_card/smart_card_reader_tracker_factory.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/permissions/permission_request_manager.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "url/origin.h"

namespace {
constexpr char kReaderNameKey[] = "reader-name";

static base::Value::Dict ReaderNameToValue(const std::string& reader_name) {
  base::Value::Dict value;
  value.Set(kReaderNameKey, reader_name);
  return value;
}

}  // anonymous namespace

class SmartCardPermissionContext::OneTimeObserver
    : public OneTimePermissionsTrackerObserver {
 public:
  explicit OneTimeObserver(SmartCardPermissionContext& permission_context)
      : permission_context_(permission_context) {
    observation_.Observe(OneTimePermissionsTrackerFactory::GetForBrowserContext(
        &permission_context.profile_.get()));
  }
  void OnLastPageFromOriginClosed(const url::Origin& origin) override {
    permission_context_->RevokeEphemeralPermissionsForOrigin(origin);
  }

 private:
  base::ScopedObservation<OneTimePermissionsTracker,
                          OneTimePermissionsTrackerObserver>
      observation_{this};
  base::raw_ref<SmartCardPermissionContext> permission_context_;
};

class SmartCardPermissionContext::PowerSuspendObserver
    : public base::PowerSuspendObserver {
 public:
  explicit PowerSuspendObserver(SmartCardPermissionContext& permission_context)
      : permission_context_(permission_context) {
    base::PowerMonitor::GetInstance()->AddPowerSuspendObserver(this);
  }

  ~PowerSuspendObserver() override {
    base::PowerMonitor::GetInstance()->RemovePowerSuspendObserver(this);
  }

  void OnSuspend() override {
    permission_context_->RevokeEphemeralPermissions();
  }

 private:
  base::raw_ref<SmartCardPermissionContext> permission_context_;
};

class SmartCardPermissionContext::ReaderObserver
    : public SmartCardReaderTracker::Observer {
 public:
  explicit ReaderObserver(SmartCardPermissionContext& permission_context)
      : permission_context_(permission_context) {}

  void Reset(std::vector<SmartCardReaderTracker::ReaderInfo> info_list) {
    known_info_map_.clear();
    for (SmartCardReaderTracker::ReaderInfo& info : info_list) {
      std::string name = info.name;
      known_info_map_.emplace(std::move(name), std::move(info));
    }
  }

  void OnReaderRemoved(const std::string& reader_name) override {
    known_info_map_.erase(reader_name);
    permission_context_->RevokeEphemeralPermissionsForReader(reader_name);
  }

  void OnReaderChanged(
      const SmartCardReaderTracker::ReaderInfo& reader_info) override {
    // Compare the new reader state with its previous one to find out whether
    // its card has been removed. If so, notify the PermissionContext.

    auto it = known_info_map_.find(reader_info.name);
    if (it == known_info_map_.end()) {
      known_info_map_.emplace(reader_info.name, reader_info);
      return;
    }

    SmartCardReaderTracker::ReaderInfo& known_info = it->second;

    // Either the card was removed or there was both a removal and an insertion
    // in the meantime (meaning that reader_info.present could still be true).
    // Note that event_count is not supported in all platforms.
    const bool card_removed =
        known_info.present &&
        (!reader_info.present ||
         (known_info.event_count < reader_info.event_count));

    // Update known_info.
    known_info = reader_info;

    if (card_removed) {
      permission_context_->RevokeEphemeralPermissionsForReader(
          reader_info.name);
    }
  }

  std::map<std::string, SmartCardReaderTracker::ReaderInfo> known_info_map_;
  base::raw_ref<SmartCardPermissionContext> permission_context_;
};

SmartCardPermissionContext::SmartCardPermissionContext(Profile* profile)
    : ObjectPermissionContextBase(
          ContentSettingsType::SMART_CARD_GUARD,
          ContentSettingsType::SMART_CARD_DATA,
          HostContentSettingsMapFactory::GetForProfile(profile)),
      reader_observer_(std::make_unique<ReaderObserver>(*this)),
      profile_(*profile),
      weak_ptr_factory_(this) {}

SmartCardPermissionContext::~SmartCardPermissionContext() = default;

std::string SmartCardPermissionContext::GetKeyForObject(
    const base::Value::Dict& object) {
  if (!IsValidObject(object)) {
    return std::string();
  }

  return *object.FindString(kReaderNameKey);
}

bool SmartCardPermissionContext::HasReaderPermission(
    content::RenderFrameHost& render_frame_host,
    const std::string& reader_name) {
  return HasReaderPermission(
      render_frame_host.GetMainFrame()->GetLastCommittedOrigin(), reader_name);
}

bool SmartCardPermissionContext::HasReaderPermission(
    const url::Origin& origin,
    const std::string& reader_name) {
  if (!CanRequestObjectPermission(origin)) {
    return false;
  }

  return ephemeral_grants_[origin].contains(reader_name) ||
         HasPersistentReaderPermission(origin, reader_name);
}

void SmartCardPermissionContext::RequestReaderPermisssion(
    content::RenderFrameHost& render_frame_host,
    const std::string& reader_name,
    RequestReaderPermissionCallback callback) {
  const url::Origin& origin =
      render_frame_host.GetMainFrame()->GetLastCommittedOrigin();

  auto* web_contents =
      content::WebContents::FromRenderFrameHost(&render_frame_host);

  permissions::PermissionRequestManager* permission_request_manager =
      permissions::PermissionRequestManager::FromWebContents(web_contents);
  if (!permission_request_manager) {
    LOG(ERROR) << "Cannot request permission: no PermissionRequestManager";
    std::move(callback).Run(false);
    return;
  }

  if (!CanRequestObjectPermission(origin)) {
    std::move(callback).Run(false);
    return;
  }

  // Regarding ownership: The request will delete itself once the request
  // manager notifies that it can do so.
  auto* permission_request = new SmartCardPermissionRequest(
      origin, reader_name,
      base::BindOnce(&SmartCardPermissionContext::OnPermissionRequestDecided,
                     weak_ptr_factory_.GetWeakPtr(), origin, reader_name,
                     std::move(callback)));

  permission_request_manager->AddRequest(&render_frame_host,
                                         permission_request);
}

void SmartCardPermissionContext::GrantEphemeralReaderPermission(
    const url::Origin& origin,
    const std::string& reader_name) {
  CHECK(!HasReaderPermission(origin, reader_name));
  ephemeral_grants_[origin].insert(reader_name);

  if (!power_suspend_observer_) {
    power_suspend_observer_ = std::make_unique<PowerSuspendObserver>(*this);
  }

  if (!one_time_observer_) {
    one_time_observer_ = std::make_unique<OneTimeObserver>(*this);
  }

  GetReaderTracker().Start(
      reader_observer_.get(),
      base::BindOnce(&SmartCardPermissionContext::OnTrackingStarted,
                     weak_ptr_factory_.GetWeakPtr()));
}

void SmartCardPermissionContext::GrantPersistentReaderPermission(
    const url::Origin& origin,
    const std::string& reader_name) {
  CHECK(!HasReaderPermission(origin, reader_name));
  GrantObjectPermission(origin, ReaderNameToValue(reader_name));
}

bool SmartCardPermissionContext::IsValidObject(
    const base::Value::Dict& object) {
  if (object.size() != 1) {
    return false;
  }

  const std::string* reader_name = object.FindString(kReaderNameKey);
  return reader_name && !reader_name->empty();
}

std::u16string SmartCardPermissionContext::GetObjectDisplayName(
    const base::Value::Dict& object) {
  const std::string* reader_name = object.FindString(kReaderNameKey);
  CHECK(reader_name);
  return base::UTF8ToUTF16(*reader_name);
}

bool SmartCardPermissionContext::HasPersistentReaderPermission(
    const url::Origin& origin,
    const std::string& reader_name) {
  for (const auto& object :
       ObjectPermissionContextBase::GetGrantedObjects(origin)) {
    const base::Value::Dict& reader_value = object->value;

    // Objects provided by the parent class can be assumed valid.
    CHECK(IsValidObject(reader_value));

    if (reader_name != *reader_value.FindString(kReaderNameKey)) {
      continue;
    }

    return true;
  }
  return false;
}

void SmartCardPermissionContext::RevokeEphemeralPermissionsForReader(
    const std::string& reader_name) {
  for (auto it = ephemeral_grants_.begin(); it != ephemeral_grants_.end();) {
    std::set<std::string>& reader_set = it->second;

    reader_set.erase(reader_name);

    if (reader_set.empty()) {
      it = ephemeral_grants_.erase(it);
    } else {
      ++it;
    }
  }

  if (ephemeral_grants_.empty()) {
    StopObserving();
  }
}

void SmartCardPermissionContext::RevokeEphemeralPermissionsForOrigin(
    const url::Origin& origin) {
  ephemeral_grants_.erase(origin);

  if (ephemeral_grants_.empty()) {
    StopObserving();
  }
}

void SmartCardPermissionContext::RevokeEphemeralPermissions() {
  if (ephemeral_grants_.empty()) {
    return;
  }

  ephemeral_grants_.clear();
  StopObserving();
}

void SmartCardPermissionContext::RevokeAllPermissions() {
  for (auto& origin : GetOriginsWithGrants()) {
    RevokeObjectPermissions(origin);
  }
  RevokeEphemeralPermissions();
}

void SmartCardPermissionContext::RevokePersistentPermission(
    const std::string& reader_name,
    const url::Origin& origin) {
  RevokeObjectPermission(origin, ReaderNameToValue(reader_name));
}

SmartCardPermissionContext::ReaderGrants::ReaderGrants(
    const std::string& reader_name,
    const std::vector<url::Origin>& origins)
    : reader_name(reader_name), origins(origins) {}
SmartCardPermissionContext::ReaderGrants::~ReaderGrants() = default;
SmartCardPermissionContext::ReaderGrants::ReaderGrants(
    const ReaderGrants& other) = default;
bool SmartCardPermissionContext::ReaderGrants::operator==(
    const ReaderGrants& other) const = default;

std::vector<SmartCardPermissionContext::ReaderGrants>
SmartCardPermissionContext::GetPersistentReaderGrants() {
  std::map<std::string, std::set<url::Origin>> reader_grants;
  for (const auto& object : GetAllGrantedObjects()) {
    const base::Value::Dict& reader_value = object->value;

    CHECK(IsValidObject(reader_value));

    reader_grants[*reader_value.FindString(kReaderNameKey)].insert(
        url::Origin::Create(object->origin));
  }

  return base::ToVector(
      reader_grants, [](const auto& reader_grants) -> ReaderGrants {
        return {reader_grants.first, base::ToVector(reader_grants.second)};
      });
}

void SmartCardPermissionContext::OnTrackingStarted(
    std::optional<std::vector<SmartCardReaderTracker::ReaderInfo>> info_list) {
  if (!info_list) {
    // PC/SC failed on us.
    LOG(ERROR) << "Failed to start reader tracking.";
    return;
  }
  reader_observer_->Reset(std::move(*info_list));
}

void SmartCardPermissionContext::StopObserving() {
  GetReaderTracker().Stop(reader_observer_.get());
  one_time_observer_.reset();
  power_suspend_observer_.reset();
}

SmartCardReaderTracker& SmartCardPermissionContext::GetReaderTracker() const {
  return SmartCardReaderTrackerFactory::GetForProfile(*profile_);
}

void SmartCardPermissionContext::OnPermissionRequestDecided(
    const url::Origin& origin,
    const std::string& reader_name,
    RequestReaderPermissionCallback callback,
    SmartCardPermissionRequest::Result result) {
  switch (result) {
    case SmartCardPermissionRequest::Result::kAllowOnce:
      GrantEphemeralReaderPermission(origin, reader_name);
      std::move(callback).Run(true);
      break;
    case SmartCardPermissionRequest::Result::kAllowAlways:
      GrantPersistentReaderPermission(origin, reader_name);
      std::move(callback).Run(true);
      break;
    case SmartCardPermissionRequest::Result::kDontAllow:
      std::move(callback).Run(false);
      break;
  }
}
