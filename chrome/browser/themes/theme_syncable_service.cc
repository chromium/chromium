// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/themes/theme_syncable_service.h"

#include <stddef.h>

#include <string>
#include <utility>

#include "base/containers/adapters.h"
#include "base/observer_list.h"
#include "base/one_shot_event.h"
#include "base/strings/stringprintf.h"
#include "base/version.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/common/extensions/sync_helper.h"
#include "components/sync/model/sync_change_processor.h"
#include "components/sync/protocol/entity_specifics.pb.h"
#include "components/sync/protocol/theme_specifics.pb.h"
#include "extensions/browser/disable_reason.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/manifest_url_handlers.h"

using std::string;

namespace {

bool IsTheme(const extensions::Extension* extension,
             content::BrowserContext* context) {
  return extension->is_theme();
}

}  // namespace

// "Current" is part of the name for historical reasons, shouldn't be changed.
const char ThemeSyncableService::kSyncEntityClientTag[] = "current_theme";
const char ThemeSyncableService::kSyncEntityTitle[] = "Current Theme";

ThemeSyncableService::ThemeSyncableService(Profile* profile,
                                           ThemeService* theme_service)
    : profile_(profile),
      theme_service_(theme_service),
      use_system_theme_by_default_(false) {
  DCHECK(theme_service_);
  theme_service_->AddObserver(this);
}

ThemeSyncableService::~ThemeSyncableService() {
  theme_service_->RemoveObserver(this);
}

void ThemeSyncableService::OnThemeChanged() {
  if (sync_processor_.get()) {
    sync_pb::ThemeSpecifics current_specifics;
    if (!GetThemeSpecificsFromCurrentTheme(&current_specifics))
      return;  // Current theme is unsyncable.
    ProcessNewTheme(syncer::SyncChange::ACTION_UPDATE, current_specifics);
    use_system_theme_by_default_ =
        current_specifics.use_system_theme_by_default();
  }
}

void ThemeSyncableService::AddObserver(
    ThemeSyncableService::Observer* observer) {
  observer_list_.AddObserver(observer);
}

void ThemeSyncableService::RemoveObserver(
    ThemeSyncableService::Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

void ThemeSyncableService::NotifyOnSyncStartedForTesting(
    ThemeSyncState startup_state) {
  NotifyOnSyncStarted(startup_state);
}

absl::optional<ThemeSyncableService::ThemeSyncState>
ThemeSyncableService::GetThemeSyncStartState() {
  return startup_state_;
}

void ThemeSyncableService::WaitUntilReadyToSync(base::OnceClosure done) {
  extensions::ExtensionSystem::Get(profile_)->ready().Post(FROM_HERE,
                                                           std::move(done));
}

absl::optional<syncer::ModelError>
ThemeSyncableService::MergeDataAndStartSyncing(
    syncer::ModelType type,
    const syncer::SyncDataList& initial_sync_data,
    std::unique_ptr<syncer::SyncChangeProcessor> sync_processor,
    std::unique_ptr<syncer::SyncErrorFactory> error_handler) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!sync_processor_.get());
  DCHECK(sync_processor.get());
  DCHECK(error_handler.get());

  sync_processor_ = std::move(sync_processor);
  sync_error_handler_ = std::move(error_handler);

  if (initial_sync_data.size() > 1) {
    sync_error_handler_->CreateAndUploadError(
        FROM_HERE,
        base::StringPrintf("Received %d theme specifics.",
                           static_cast<int>(initial_sync_data.size())));
  }

  sync_pb::ThemeSpecifics current_specifics;
  if (!GetThemeSpecificsFromCurrentTheme(&current_specifics)) {
    // Current theme is unsyncable - don't overwrite from sync data, and don't
    // save the unsyncable theme to sync data.
    NotifyOnSyncStarted(ThemeSyncState::kFailed);
    return absl::nullopt;
  }

  // Find the last SyncData that has theme data and set the current theme from
  // it. If SyncData doesn't have a theme, but there is a current theme, it will
  // not reset it.
  for (const syncer::SyncData& sync_data : base::Reversed(initial_sync_data)) {
    if (sync_data.GetSpecifics().has_theme()) {
      if (!HasNonDefaultTheme(current_specifics) ||
          HasNonDefaultTheme(sync_data.GetSpecifics().theme())) {
        ThemeSyncState startup_state =
            MaybeSetTheme(current_specifics, sync_data);
        NotifyOnSyncStarted(startup_state);
        return absl::nullopt;
      }
    }
  }

  // No theme specifics are found. Create one according to current theme.
  absl::optional<syncer::ModelError> error =
      ProcessNewTheme(syncer::SyncChange::ACTION_ADD, current_specifics);
  NotifyOnSyncStarted(ThemeSyncState::kApplied);
  return error;
}

void ThemeSyncableService::StopSyncing(syncer::ModelType type) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK_EQ(type, syncer::THEMES);

  sync_processor_.reset();
  sync_error_handler_.reset();
}

syncer::SyncDataList ThemeSyncableService::GetAllSyncDataForTesting(
    syncer::ModelType type) const {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK_EQ(type, syncer::THEMES);

  syncer::SyncDataList list;
  sync_pb::EntitySpecifics entity_specifics;
  if (GetThemeSpecificsFromCurrentTheme(entity_specifics.mutable_theme())) {
    list.push_back(syncer::SyncData::CreateLocalData(
        kSyncEntityClientTag, kSyncEntityTitle, entity_specifics));
  }
  return list;
}

absl::optional<syncer::ModelError> ThemeSyncableService::ProcessSyncChanges(
    const base::Location& from_here,
    const syncer::SyncChangeList& change_list) {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (!sync_processor_.get()) {
    return syncer::ModelError(FROM_HERE,
                              "Theme syncable service is not started.");
  }

  // TODO(akalin): Normally, we should only have a single change and
  // it should be an update.  However, the syncapi may occasionally
  // generates multiple changes.  When we fix syncapi to not do that,
  // we can remove the extra logic below.  See:
  // http://code.google.com/p/chromium/issues/detail?id=41696 .
  if (change_list.size() != 1) {
    string err_msg = base::StringPrintf("Received %d theme changes: ",
                                        static_cast<int>(change_list.size()));
    for (size_t i = 0; i < change_list.size(); ++i) {
      base::StringAppendF(&err_msg, "[%s] ", change_list[i].ToString().c_str());
    }
    sync_error_handler_->CreateAndUploadError(FROM_HERE, err_msg);
  } else if (change_list.begin()->change_type() !=
          syncer::SyncChange::ACTION_ADD
      && change_list.begin()->change_type() !=
          syncer::SyncChange::ACTION_UPDATE) {
    sync_error_handler_->CreateAndUploadError(
        FROM_HERE,
        "Invalid theme change: " + change_list.begin()->ToString());
  }

  sync_pb::ThemeSpecifics current_specifics;
  if (!GetThemeSpecificsFromCurrentTheme(&current_specifics)) {
    // Current theme is unsyncable, so don't overwrite it.
    return absl::nullopt;
  }

  // Set current theme from the theme specifics of the last change of type
  // |ACTION_ADD| or |ACTION_UPDATE|.
  for (const syncer::SyncChange& theme_change : base::Reversed(change_list)) {
    if (theme_change.sync_data().GetSpecifics().has_theme() &&
        (theme_change.change_type() == syncer::SyncChange::ACTION_ADD ||
         theme_change.change_type() == syncer::SyncChange::ACTION_UPDATE)) {
      MaybeSetTheme(current_specifics, theme_change.sync_data());
      return absl::nullopt;
    }
  }

  return syncer::ModelError(FROM_HERE, "Didn't find valid theme specifics");
}

ThemeSyncableService::ThemeSyncState ThemeSyncableService::MaybeSetTheme(
    const sync_pb::ThemeSpecifics& current_specs,
    const syncer::SyncData& sync_data) {
  const sync_pb::ThemeSpecifics& sync_theme = sync_data.GetSpecifics().theme();
  use_system_theme_by_default_ = sync_theme.use_system_theme_by_default();
  DVLOG(1) << "Set current theme from specifics: " << sync_data.ToString();
  if (AreThemeSpecificsEqual(
          current_specs, sync_theme,
          theme_service_->IsSystemThemeDistinctFromDefaultTheme())) {
    DVLOG(1) << "Skip setting theme because specs are equal";
    return ThemeSyncState::kApplied;
  }
  return SetCurrentThemeFromThemeSpecifics(sync_theme);
}

ThemeSyncableService::ThemeSyncState
ThemeSyncableService::SetCurrentThemeFromThemeSpecifics(
    const sync_pb::ThemeSpecifics& theme_specifics) {
  if (theme_specifics.use_custom_theme()) {
    // TODO(akalin): Figure out what to do about third-party themes
    // (i.e., those not on either Google gallery).
    string id(theme_specifics.custom_theme_id());
    GURL update_url(theme_specifics.custom_theme_update_url());
    DVLOG(1) << "Applying theme " << id << " with update_url " << update_url;
    extensions::ExtensionService* extension_service =
        extensions::ExtensionSystem::Get(profile_)->extension_service();
    CHECK(extension_service);
    extensions::ExtensionRegistry* extension_registry =
        extensions::ExtensionRegistry::Get(profile_);
    CHECK(extension_registry);
    const extensions::Extension* extension =
        extension_registry->GetExtensionById(
            id, extensions::ExtensionRegistry::EVERYTHING);
    if (extension) {
      if (!extension->is_theme()) {
        DVLOG(1) << "Extension " << id << " is not a theme; aborting";
        return ThemeSyncState::kFailed;
      }
      int disabled_reasons =
          extensions::ExtensionPrefs::Get(profile_)->GetDisableReasons(id);
      if (!extension_service->IsExtensionEnabled(id) &&
          disabled_reasons != extensions::disable_reason::DISABLE_USER_ACTION) {
        DVLOG(1) << "Theme " << id << " is disabled with reason "
                 << disabled_reasons << "; aborting";
        return ThemeSyncState::kFailed;
      }
      // An enabled theme extension with the given id was found, so
      // just set the current theme to it.
      theme_service_->SetTheme(extension);
      return ThemeSyncState::kApplied;
    }

    // No extension with this id exists -- we must install it; we do
    // so by adding it as a pending extension and then triggering an
    // auto-update cycle.
    const bool kRemoteInstall = false;
    if (!extension_service->pending_extension_manager()->AddFromSync(
            id, update_url, base::Version(), &IsTheme, kRemoteInstall)) {
      LOG(WARNING) << "Could not add pending extension for " << id;
      return ThemeSyncState::kFailed;
    }
    extension_service->CheckForUpdatesSoon();
    // Return that the call triggered an extension theme installation.
    return ThemeSyncState::kWaitingForExtensionInstallation;
  }

  if (theme_specifics.has_autogenerated_theme()) {
    DVLOG(1) << "Applying autogenerated theme";
    theme_service_->BuildAutogeneratedThemeFromColor(
        theme_specifics.autogenerated_theme().color());
    return ThemeSyncState::kApplied;
  }

  if (theme_specifics.use_system_theme_by_default()) {
    DVLOG(1) << "Switch to use system theme";
    theme_service_->UseSystemTheme();
    return ThemeSyncState::kApplied;
  }

  DVLOG(1) << "Switch to use default theme";
  theme_service_->UseDefaultTheme();
  return ThemeSyncState::kApplied;
}

bool ThemeSyncableService::GetThemeSpecificsFromCurrentTheme(
    sync_pb::ThemeSpecifics* theme_specifics) const {
  const extensions::Extension* current_extension =
      theme_service_->UsingExtensionTheme() &&
              !theme_service_->UsingDefaultTheme()
          ? extensions::ExtensionRegistry::Get(profile_)
                ->enabled_extensions()
                .GetByID(theme_service_->GetThemeID())
          : nullptr;
  if (current_extension &&
      !extensions::sync_helper::IsSyncable(current_extension)) {
    DVLOG(1) << "Ignoring non-syncable extension: " << current_extension->id();
    return false;
  }

  // If theme was set through policy, it should be unsyncable.
  if (theme_service_->UsingPolicyTheme())
    return false;

  theme_specifics->Clear();
  theme_specifics->set_use_custom_theme(false);

  if (current_extension) {
    // Using custom theme and it's an extension.
    DCHECK(current_extension->is_theme());
    theme_specifics->set_use_custom_theme(true);
    theme_specifics->set_custom_theme_name(current_extension->name());
    theme_specifics->set_custom_theme_id(current_extension->id());
    theme_specifics->set_custom_theme_update_url(
        extensions::ManifestURL::GetUpdateURL(current_extension).spec());
  }

  if (theme_service_->UsingAutogeneratedTheme()) {
    // Using custom theme and it's autogenerated from color.
    theme_specifics->set_use_custom_theme(false);
    theme_specifics->mutable_autogenerated_theme()->set_color(
        theme_service_->GetAutogeneratedThemeColor());
  }

  if (theme_service_->IsSystemThemeDistinctFromDefaultTheme()) {
    // On platform where system theme is different from default theme, set
    // use_system_theme_by_default to true if system theme is used, false
    // if default system theme is used. Otherwise restore it to value in sync.
    if (theme_service_->UsingSystemTheme()) {
      theme_specifics->set_use_system_theme_by_default(true);
    } else if (theme_service_->UsingDefaultTheme()) {
      theme_specifics->set_use_system_theme_by_default(false);
    } else {
      theme_specifics->set_use_system_theme_by_default(
          use_system_theme_by_default_);
    }
  } else {
    // Restore use_system_theme_by_default when platform doesn't distinguish
    // between default theme and system theme.
    theme_specifics->set_use_system_theme_by_default(
        use_system_theme_by_default_);
  }
  return true;
}

/* static */
bool ThemeSyncableService::AreThemeSpecificsEqual(
    const sync_pb::ThemeSpecifics& a,
    const sync_pb::ThemeSpecifics& b,
    bool is_system_theme_distinct_from_default_theme) {
  if (HasNonDefaultTheme(a) != HasNonDefaultTheme(b)) {
    return false;
  }

  if (a.use_custom_theme()) {
    // We're using an extensions theme, so simply compare IDs since those
    // are guaranteed unique.
    return a.use_custom_theme() == b.use_custom_theme() &&
           a.custom_theme_id() == b.custom_theme_id();
  } else if (a.has_autogenerated_theme()) {
    return a.has_autogenerated_theme() == b.has_autogenerated_theme() &&
           a.autogenerated_theme().color() == b.autogenerated_theme().color();
  } else if (is_system_theme_distinct_from_default_theme) {
    // We're not using a custom theme, but we care about system
    // vs. default.
    return a.use_system_theme_by_default() == b.use_system_theme_by_default();
  } else {
    // We're not using a custom theme, and we don't care about system
    // vs. default.
    return true;
  }
}

bool ThemeSyncableService::HasNonDefaultTheme(
    const sync_pb::ThemeSpecifics& theme_specifics) {
  return theme_specifics.use_custom_theme() ||
         theme_specifics.has_autogenerated_theme();
}

absl::optional<syncer::ModelError> ThemeSyncableService::ProcessNewTheme(
    syncer::SyncChange::SyncChangeType change_type,
    const sync_pb::ThemeSpecifics& theme_specifics) {
  syncer::SyncChangeList changes;
  sync_pb::EntitySpecifics entity_specifics;
  entity_specifics.mutable_theme()->CopyFrom(theme_specifics);

  changes.emplace_back(
      FROM_HERE, change_type,
      syncer::SyncData::CreateLocalData(kSyncEntityClientTag, kSyncEntityTitle,
                                        entity_specifics));

  DVLOG(1) << "Update theme specifics from current theme: "
      << changes.back().ToString();

  return sync_processor_->ProcessSyncChanges(FROM_HERE, changes);
}

void ThemeSyncableService::NotifyOnSyncStarted(ThemeSyncState startup_state) {
  // Keep the state for later calls to GetThemeSyncStartState().
  startup_state_ = startup_state;

  for (Observer& observer : observer_list_)
    observer.OnThemeSyncStarted(startup_state);
}
