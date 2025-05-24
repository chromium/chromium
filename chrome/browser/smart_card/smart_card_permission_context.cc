// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/smart_card/smart_card_permission_context.h"

#include <algorithm>
#include <iterator>
#include <vector>

#include "base/check.h"
#include "base/check_deref.h"
#include "base/check_op.h"
#include "base/containers/to_vector.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/power_monitor/power_monitor.h"
#include "base/power_monitor/power_observer.h"
#include "base/scoped_observation.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/values.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/permissions/one_time_permissions_tracker.h"
#include "chrome/browser/permissions/one_time_permissions_tracker_factory.h"
#include "chrome/browser/permissions/one_time_permissions_tracker_observer.h"
#include "chrome/browser/permissions/permission_decision_auto_blocker_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/smart_card/smart_card_histograms.h"
#include "chrome/browser/smart_card/smart_card_reader_tracker.h"
#include "chrome/browser/smart_card/smart_card_reader_tracker_factory.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/permissions/permission_context_base.h"
#include "components/permissions/permission_decision_auto_blocker.h"
#include "components/permissions/permission_request_manager.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "third_party/blink/public/common/features.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace {
constexpr char kReaderNameKey[] = "reader-name";

template <typename StringType>
static base::Value::Dict ReaderNameToValue(const StringType& reader_name) {
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
    RecordSmartCardOneTimePermissionExpiryReason(
        SmartCardOneTimePermissionExpiryReason::
            kSmartCardPermissionExpiredLastWindowClosed);
  }

  void OnAllTabsInBackgroundTimerExpired(
      const url::Origin& origin,
      const BackgroundExpiryType& expiry_type) override {
    if (expiry_type == BackgroundExpiryType::kTimeout) {
      permission_context_->RevokeEphemeralPermissionsForOrigin(origin);
      // Record histogram even if no permissions were revoked - for the purpose
      // of a dry run.
      RecordSmartCardOneTimePermissionExpiryReason(
          SmartCardOneTimePermissionExpiryReason::
              kSmartCardPermissionExpiredAllWindowsInTheBackgroundTimeout);
    }
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
    RecordSmartCardOneTimePermissionExpiryReason(
        SmartCardOneTimePermissionExpiryReason::
            kSmartCardPermissionExpiredSystemSuspended);
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
    RecordSmartCardOneTimePermissionExpiryReason(
        SmartCardOneTimePermissionExpiryReason::
            kSmartCardPermissionExpiredReaderRemoved);
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
      RecordSmartCardOneTimePermissionExpiryReason(
          SmartCardOneTimePermissionExpiryReason::
              kSmartCardPermissionExpiredCardRemoved);
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
      weak_ptr_factory_(this) {
  permission_observation_.Observe(this);
}

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
    return IsAllowlistedByPolicy(origin);
  }

  return ephemeral_grants_with_expiry_[origin].contains(reader_name) ||
         HasPersistentReaderPermission(origin, reader_name);
}

bool SmartCardPermissionContext::IsAllowlistedByPolicy(
    const url::Origin& origin) const {
  if (!guard_content_settings_type_) {
    return false;
  }

  content_settings::SettingInfo setting_info;
  auto content_setting =
      HostContentSettingsMapFactory::GetForProfile(&profile_.get())
          ->GetContentSetting(origin.GetURL(), GURL(),
                              ContentSettingsType::SMART_CARD_GUARD,
                              &setting_info);
  return setting_info.source == content_settings::SettingSource::kPolicy &&
         content_setting == CONTENT_SETTING_ALLOW;
}

bool SmartCardPermissionContext::CanRequestObjectPermission(
    const url::Origin& origin) const {
  CHECK(guard_content_settings_type_);
  if (CHECK_DEREF(
          PermissionDecisionAutoBlockerFactory::GetForProfile(&profile_.get()))
          .IsEmbargoed(origin.GetURL(), *guard_content_settings_type_)) {
    return false;
  }
  return ObjectPermissionContextBase::CanRequestObjectPermission(origin);
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
    std::move(callback).Run(IsAllowlistedByPolicy(origin));
    return;
  }

  auto permission_request = std::make_unique<SmartCardPermissionRequest>(
      origin, reader_name,
      base::BindOnce(&SmartCardPermissionContext::OnPermissionRequestDecided,
                     weak_ptr_factory_.GetWeakPtr(), origin, reader_name,
                     std::move(callback)));

  permission_request_manager->AddRequest(&render_frame_host,
                                         std::move(permission_request));
}

void SmartCardPermissionContext::GrantEphemeralReaderPermission(
    const url::Origin& origin,
    const std::string& reader_name) {
  CHECK(!HasReaderPermission(origin, reader_name));
  ephemeral_grants_with_expiry_[origin].emplace(
      reader_name,
      base::Time::Now() + permissions::kOneTimePermissionMaximumLifetime);

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
  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&SmartCardPermissionContext::
                         RevokeEphemeralPermissionIfLongTimeoutOccured,
                     weak_ptr_factory_.GetWeakPtr(), origin, reader_name),
      permissions::kOneTimePermissionMaximumLifetime);
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
  for (auto it = ephemeral_grants_with_expiry_.begin();
       it != ephemeral_grants_with_expiry_.end();) {
    auto& [origin, reader_map] = *it;

    if (reader_map.erase(reader_name)) {
      NotifyPermissionRevoked(origin);
    }

    if (reader_map.empty()) {
      it = ephemeral_grants_with_expiry_.erase(it);
    } else {
      ++it;
    }
  }

  if (ephemeral_grants_with_expiry_.empty()) {
    StopObserving();
  }
}

void SmartCardPermissionContext::RevokeEphemeralPermissionsForOrigin(
    const url::Origin& origin) {
  ephemeral_grants_with_expiry_.erase(origin);

  if (ephemeral_grants_with_expiry_.empty()) {
    StopObserving();
  }
  NotifyPermissionRevoked(origin);
}

void SmartCardPermissionContext::RevokeEphemeralPermissions() {
  if (ephemeral_grants_with_expiry_.empty()) {
    return;
  }
  std::set<url::Origin> revoked_origins;

  std::ranges::transform(ephemeral_grants_with_expiry_,
                         std::inserter(revoked_origins, revoked_origins.end()),
                         [](const auto& pair) { return pair.first; });

  ephemeral_grants_with_expiry_.clear();
  StopObserving();
  for (const auto& origin : revoked_origins) {
    NotifyPermissionRevoked(origin);
  }
}

void SmartCardPermissionContext::RevokeAllPermissions() {
  for (auto& origin : GetOriginsWithGrants()) {
    RevokeObjectPermissions(origin);
  }
  RevokeEphemeralPermissions();
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

std::vector<std::unique_ptr<SmartCardPermissionContext::Object>>
SmartCardPermissionContext::GetGrantedObjects(const url::Origin& origin) {
  std::vector<std::unique_ptr<Object>> objects =
      ObjectPermissionContextBase::GetGrantedObjects(origin);

  if (IsAllowlistedByPolicy(origin)) {
    objects.push_back(std::make_unique<Object>(
        origin,
        ReaderNameToValue(l10n_util::GetStringUTF16(
            IDS_SMART_CARD_POLICY_DESCRIPTION_FOR_ANY_DEVICE)),
        content_settings::SettingSource::kPolicy, IsOffTheRecord()));
  }
  return objects;
}

void SmartCardPermissionContext::OnPermissionRevoked(
    const url::Origin& origin) {
  permission_observers_.Notify(
      &content::SmartCardDelegate::PermissionObserver::OnPermissionRevoked,
      origin);
}

void SmartCardPermissionContext::AddObserver(
    content::SmartCardDelegate::PermissionObserver* observer) {
  permission_observers_.AddObserver(observer);
}

void SmartCardPermissionContext::RemoveObserver(
    content::SmartCardDelegate::PermissionObserver* observer) {
  permission_observers_.RemoveObserver(observer);
}
void SmartCardPermissionContext::RevokeEphemeralPermissionIfLongTimeoutOccured(
    const url::Origin& origin,
    const std::string& reader_name) {
  auto it_origin = ephemeral_grants_with_expiry_.find(origin);
  if (it_origin == ephemeral_grants_with_expiry_.end()) {
    return;
  }
  auto& reader_map = it_origin->second;
  auto it_reader = reader_map.find(reader_name);
  if (it_reader == reader_map.end()) {
    return;
  }

  if (base::Time::Now() >= it_reader->second) {
    reader_map.erase(it_reader);
    if (reader_map.empty()) {
      ephemeral_grants_with_expiry_.erase(it_origin);
    }
    NotifyPermissionRevoked(origin);
    RecordSmartCardOneTimePermissionExpiryReason(
        SmartCardOneTimePermissionExpiryReason::
            kSmartCardPermissionExpiredMaxLifetimeReached);
  }
}

std::vector<std::unique_ptr<SmartCardPermissionContext::Object>>
SmartCardPermissionContext::GetAllGrantedObjects() {
  auto objects = ObjectPermissionContextBase::GetAllGrantedObjects();
  const auto& allowlisted_origins = profile_->GetPrefs()->GetList(
      prefs::kManagedSmartCardConnectAllowedForUrls);
  for (const auto& allowlisted_origin : allowlisted_origins) {
    CHECK(allowlisted_origin.is_string());
    GURL url = GURL(allowlisted_origin.GetString());
    CHECK(url.is_valid());

    objects.push_back(std::make_unique<Object>(
        url::Origin::Create(url),
        ReaderNameToValue(l10n_util::GetStringUTF16(
            IDS_SMART_CARD_POLICY_DESCRIPTION_FOR_ANY_DEVICE)),
        content_settings::SettingSource::kPolicy, IsOffTheRecord()));
  }
  return objects;
}
