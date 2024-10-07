// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/themes/theme_syncable_service.h"

#include <stddef.h>

#include <string>
#include <utility>

#include "base/base64.h"
#include "base/containers/adapters.h"
#include "base/containers/fixed_flat_map.h"
#include "base/feature_list.h"
#include "base/observer_list.h"
#include "base/one_shot_event.h"
#include "base/strings/stringprintf.h"
#include "base/version.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/background/ntp_custom_background_service_constants.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_utils.h"
#include "chrome/common/extensions/sync_helper.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/sync/base/features.h"
#include "components/sync/model/sync_change_processor.h"
#include "components/sync/protocol/entity_specifics.pb.h"
#include "components/sync/protocol/proto_value_conversions.h"
#include "components/sync/protocol/theme_specifics.pb.h"
#include "components/sync_preferences/pref_service_syncable.h"
#include "components/sync_preferences/pref_service_syncable_observer.h"
#include "extensions/browser/disable_reason.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/manifest_url_handlers.h"

using std::string;

namespace {

struct ThemePrefNames {
  std::string_view syncing_pref_name;
  std::string_view non_syncing_pref_name;
};

constexpr auto kThemePrefsInMigration =
    base::MakeFixedFlatMap<ThemePrefInMigration, ThemePrefNames>({
        {ThemePrefInMigration::kBrowserColorScheme,
         {prefs::kBrowserColorSchemeDoNotUse,
          prefs::kNonSyncingBrowserColorSchemeDoNotUse}},
        {ThemePrefInMigration::kUserColor,
         {prefs::kUserColorDoNotUse, prefs::kNonSyncingUserColorDoNotUse}},
        {ThemePrefInMigration::kBrowserColorVariant,
         {prefs::kBrowserColorVariantDoNotUse,
          prefs::kNonSyncingBrowserColorVariantDoNotUse}},
        {ThemePrefInMigration::kGrayscaleThemeEnabled,
         {prefs::kGrayscaleThemeEnabledDoNotUse,
          prefs::kNonSyncingGrayscaleThemeEnabledDoNotUse}},
        {ThemePrefInMigration::kNtpCustomBackgroundDict,
         {prefs::kNtpCustomBackgroundDictDoNotUse,
          prefs::kNonSyncingNtpCustomBackgroundDictDoNotUse}},
    });

static_assert(
    kThemePrefsInMigration.size() ==
        static_cast<size_t>(ThemePrefInMigration::kLastEntry) + 1,
    "ThemePrefInMigration entry missing from kThemePrefsInMigration map.");

bool IsTheme(const extensions::Extension* extension,
             content::BrowserContext* context) {
  return extension->is_theme();
}

bool HasNonDefaultBrowserColorScheme(
    const sync_pb::ThemeSpecifics& theme_specifics) {
  return theme_specifics.has_browser_color_scheme() &&
         ProtoEnumToBrowserColorScheme(
             theme_specifics.browser_color_scheme()) !=
             ThemeService::BrowserColorScheme::kSystem;
}

base::Value::Dict SpecificsNtpBackgroundToDict(
    const sync_pb::ThemeSpecifics::NtpCustomBackground& ntp_background) {
  base::Value::Dict dict;
  if (ntp_background.has_url()) {
    dict.Set(kNtpCustomBackgroundURL, ntp_background.url());
  }
  if (ntp_background.has_attribution_line_1()) {
    dict.Set(kNtpCustomBackgroundAttributionLine1,
             ntp_background.attribution_line_1());
  }
  if (ntp_background.has_attribution_line_2()) {
    dict.Set(kNtpCustomBackgroundAttributionLine2,
             ntp_background.attribution_line_2());
  }
  if (ntp_background.has_attribution_action_url()) {
    dict.Set(kNtpCustomBackgroundAttributionActionURL,
             ntp_background.attribution_action_url());
  }
  if (ntp_background.has_collection_id()) {
    dict.Set(kNtpCustomBackgroundCollectionId, ntp_background.collection_id());
  }
  if (ntp_background.has_resume_token()) {
    dict.Set(kNtpCustomBackgroundResumeToken, ntp_background.resume_token());
  }
  if (ntp_background.has_refresh_timestamp_unix_epoch_seconds()) {
    dict.Set(kNtpCustomBackgroundRefreshTimestamp,
             static_cast<int>(
                 ntp_background.refresh_timestamp_unix_epoch_seconds()));
  }
  if (ntp_background.has_main_color()) {
    dict.Set(kNtpCustomBackgroundMainColor,
             static_cast<int>(ntp_background.main_color()));
  }
  return dict;
}

sync_pb::ThemeSpecifics::NtpCustomBackground SpecificsNtpBackgroundFromDict(
    const base::Value::Dict& dict) {
  sync_pb::ThemeSpecifics::NtpCustomBackground ntp_background;
  if (const std::string* value = dict.FindString(kNtpCustomBackgroundURL)) {
    ntp_background.set_url(*value);
  }
  if (const std::string* value =
          dict.FindString(kNtpCustomBackgroundAttributionLine1)) {
    ntp_background.set_attribution_line_1(*value);
  }
  if (const std::string* value =
          dict.FindString(kNtpCustomBackgroundAttributionLine2)) {
    ntp_background.set_attribution_line_2(*value);
  }
  if (const std::string* value =
          dict.FindString(kNtpCustomBackgroundAttributionActionURL)) {
    ntp_background.set_attribution_action_url(*value);
  }
  if (const std::string* value =
          dict.FindString(kNtpCustomBackgroundCollectionId)) {
    ntp_background.set_collection_id(*value);
  }
  if (const std::string* value =
          dict.FindString(kNtpCustomBackgroundResumeToken)) {
    ntp_background.set_resume_token(*value);
  }
  if (std::optional<int> value =
          dict.FindInt(kNtpCustomBackgroundRefreshTimestamp)) {
    ntp_background.set_refresh_timestamp_unix_epoch_seconds(*value);
  }
  if (std::optional<int> value = dict.FindInt(kNtpCustomBackgroundMainColor)) {
    ntp_background.set_main_color(*value);
  }
  return ntp_background;
}

bool AreSpecificsNtpBackgroundEquivalent(
    const sync_pb::ThemeSpecifics::NtpCustomBackground& a,
    const sync_pb::ThemeSpecifics::NtpCustomBackground& b) {
  return a.url() == b.url() && a.collection_id() == b.collection_id() &&
         a.main_color() == b.main_color();
}

}  // namespace

// "Current" is part of the name for historical reasons, shouldn't be changed.
const char ThemeSyncableService::kSyncEntityClientTag[] = "current_theme";
const char ThemeSyncableService::kSyncEntityTitle[] = "Current Theme";

std::string_view GetThemePrefNameInMigration(ThemePrefInMigration theme_pref) {
  const ThemePrefNames& theme_pref_names =
      kThemePrefsInMigration.at(theme_pref);
  return base::FeatureList::IsEnabled(syncer::kMoveThemePrefsToSpecifics)
             ? theme_pref_names.non_syncing_pref_name
             : theme_pref_names.syncing_pref_name;
}

void MigrateSyncingThemePrefsToNonSyncingIfNeeded(PrefService* prefs) {
  if (!base::FeatureList::IsEnabled(syncer::kMoveThemePrefsToSpecifics)) {
    // Clear migration flag to allow re-migration when the feature flag is
    // re-enabled.
    prefs->ClearPref(prefs::kSyncingThemePrefsMigratedToNonSyncing);
    return;
  }
  if (prefs->GetBoolean(prefs::kSyncingThemePrefsMigratedToNonSyncing)) {
    return;
  }
  for (const auto& [pref_in_migration, pref_names] : kThemePrefsInMigration) {
    if (const base::Value* value =
            prefs->GetUserPrefValue(pref_names.syncing_pref_name)) {
      prefs->Set(pref_names.non_syncing_pref_name, value->Clone());
    }
  }

  prefs->SetBoolean(prefs::kSyncingThemePrefsMigratedToNonSyncing, true);
}

class ThemeSyncableService::PrefServiceSyncableObserver
    : public sync_preferences::PrefServiceSyncableObserver {
 public:
  explicit PrefServiceSyncableObserver(
      sync_preferences::PrefServiceSyncable* prefs)
      : prefs_(prefs) {
    observation_.Observe(prefs);
    // Prefs sync might have already started.
    OnIsSyncingChanged();
  }

  void OnIsSyncingChanged() override {
    CHECK(prefs_->GetBoolean(prefs::kShouldReadIncomingSyncingThemePrefs));
    if (prefs_->IsSyncing()) {
      observation_.Reset();

      // Copy over synced pref values to the new theme prefs.
      for (const auto& [pref_in_migration, pref_names] :
           kThemePrefsInMigration) {
        if (const base::Value* value =
                prefs_->GetUserPrefValue(pref_names.syncing_pref_name)) {
          // User color pref needs another pref to be set to be detected.
          if (pref_in_migration == ThemePrefInMigration::kUserColor) {
            prefs_->SetString(prefs::kCurrentThemeID,
                              ThemeService::kUserColorThemeID);
          }
          prefs_->Set(pref_names.non_syncing_pref_name, value->Clone());
        }
      }
      prefs_->SetBoolean(prefs::kShouldReadIncomingSyncingThemePrefs, false);
    }
  }

 private:
  base::ScopedObservation<sync_preferences::PrefServiceSyncable,
                          sync_preferences::PrefServiceSyncableObserver>
      observation_{this};
  raw_ptr<sync_preferences::PrefServiceSyncable> prefs_;
};

ThemeSyncableService::ThemeSyncableService(Profile* profile,
                                           ThemeService* theme_service)
    : profile_(profile),
      theme_service_(theme_service),
      use_system_theme_by_default_(false) {
  DCHECK(theme_service_);
  theme_service_->AddObserver(this);

  // `profile_` can be null in tests.
  if (!profile_ || !profile_->GetPrefs()) {
    return;
  }

  sync_preferences::PrefServiceSyncable* prefs =
      static_cast<sync_preferences::PrefServiceSyncable*>(profile_->GetPrefs());
  if (base::FeatureList::IsEnabled(syncer::kMoveThemePrefsToSpecifics)) {
    // Listen to NtpCustomBackgroundDict pref changes. This is done because
    // ThemeService doesn't convey ntp background change notifications.
    pref_change_registrar_.Init(prefs);
    pref_change_registrar_.Add(
        prefs::kNonSyncingNtpCustomBackgroundDictDoNotUse,
        base::BindRepeating(&ThemeSyncableService::OnThemeChanged,
                            base::Unretained(this)));

    if (prefs->GetBoolean(prefs::kShouldReadIncomingSyncingThemePrefs)) {
      // ThemeSyncableService instance is destroyed upon ThemeService::Shutdown.
      // So `prefs` outlives this.
      pref_service_syncable_observer_ =
          std::make_unique<PrefServiceSyncableObserver>(prefs);
    }
  } else {
    // Reset flag to allow reading the syncing prefs once again when
    // kMoveThemePrefsToSpecifics feature is re-enabled.
    prefs->SetBoolean(prefs::kShouldReadIncomingSyncingThemePrefs, true);
  }
}

ThemeSyncableService::~ThemeSyncableService() {
  theme_service_->RemoveObserver(this);
}

void ThemeSyncableService::OnThemeChanged() {
  if (sync_processor_.get() && !processing_syncer_changes_ &&
      IsCurrentThemeSyncable()) {
    const sync_pb::ThemeSpecifics current_specifics =
        GetThemeSpecificsFromCurrentTheme();
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

std::optional<ThemeSyncableService::ThemeSyncState>
ThemeSyncableService::GetThemeSyncStartState() {
  return startup_state_;
}

void ThemeSyncableService::WaitUntilReadyToSync(base::OnceClosure done) {
  extensions::ExtensionSystem::Get(profile_)->ready().Post(FROM_HERE,
                                                           std::move(done));
}

std::optional<syncer::ModelError>
ThemeSyncableService::MergeDataAndStartSyncing(
    syncer::DataType type,
    const syncer::SyncDataList& initial_sync_data,
    std::unique_ptr<syncer::SyncChangeProcessor> sync_processor) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!sync_processor_.get());
  DCHECK(sync_processor.get());

  sync_processor_ = std::move(sync_processor);

  if (initial_sync_data.size() > 1) {
    return syncer::ModelError(
        FROM_HERE,
        base::StringPrintf("Received %d theme specifics.",
                           static_cast<int>(initial_sync_data.size())));
  }

  sync_pb::ThemeSpecifics current_specifics =
      GetThemeSpecificsFromCurrentTheme();
  if (base::FeatureList::IsEnabled(syncer::kSeparateLocalAndAccountThemes)) {
    // Save current theme specifics to pref. This is used to restore the local
    // theme upon signout.
    profile_->GetPrefs()->SetString(
        prefs::kSavedLocalTheme,
        base::Base64Encode(current_specifics.SerializeAsString()));
  }

  if (!IsCurrentThemeSyncable()) {
    // Current theme is unsyncable - don't overwrite from sync data, and don't
    // save the unsyncable theme to sync data.
    NotifyOnSyncStarted(ThemeSyncState::kFailed);
    return std::nullopt;
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
        return std::nullopt;
      }
    }
  }

  // No theme specifics are found. Create one according to current theme.
  std::optional<syncer::ModelError> error =
      ProcessNewTheme(syncer::SyncChange::ACTION_ADD, current_specifics);
  NotifyOnSyncStarted(ThemeSyncState::kApplied);
  return error;
}

void ThemeSyncableService::StopSyncing(syncer::DataType type) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK_EQ(type, syncer::THEMES);

  sync_processor_.reset();
}

syncer::SyncDataList ThemeSyncableService::GetAllSyncDataForTesting(
    syncer::DataType type) const {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK_EQ(type, syncer::THEMES);

  syncer::SyncDataList list;
  if (IsCurrentThemeSyncable()) {
    sync_pb::EntitySpecifics entity_specifics;
    *entity_specifics.mutable_theme() = GetThemeSpecificsFromCurrentTheme();
    list.push_back(syncer::SyncData::CreateLocalData(
        kSyncEntityClientTag, kSyncEntityTitle, entity_specifics));
  }
  return list;
}

std::optional<syncer::ModelError> ThemeSyncableService::ProcessSyncChanges(
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
    return syncer::ModelError(FROM_HERE, err_msg);
  }
  if (change_list.begin()->change_type() != syncer::SyncChange::ACTION_ADD &&
      change_list.begin()->change_type() != syncer::SyncChange::ACTION_UPDATE) {
    return syncer::ModelError(
        FROM_HERE, "Invalid theme change: " + change_list.begin()->ToString());
  }

  if (!IsCurrentThemeSyncable()) {
    // Current theme is unsyncable, so don't overwrite it.
    return std::nullopt;
  }

  // Set current theme from the theme specifics of the last change of type
  // |ACTION_ADD| or |ACTION_UPDATE|.
  for (const syncer::SyncChange& theme_change : base::Reversed(change_list)) {
    if (theme_change.sync_data().GetSpecifics().has_theme() &&
        (theme_change.change_type() == syncer::SyncChange::ACTION_ADD ||
         theme_change.change_type() == syncer::SyncChange::ACTION_UPDATE)) {
      MaybeSetTheme(GetThemeSpecificsFromCurrentTheme(),
                    theme_change.sync_data());
      return std::nullopt;
    }
  }

  return syncer::ModelError(FROM_HERE, "Didn't find valid theme specifics");
}

base::WeakPtr<syncer::SyncableService> ThemeSyncableService::AsWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

ThemeSyncableService::ThemeSyncState ThemeSyncableService::MaybeSetTheme(
    const sync_pb::ThemeSpecifics& current_specs,
    const syncer::SyncData& sync_data) {
  const sync_pb::ThemeSpecifics& theme_specifics =
      sync_data.GetSpecifics().theme();
  use_system_theme_by_default_ = theme_specifics.use_system_theme_by_default();
  DVLOG(1) << "Set current theme from specifics: " << sync_data.ToString();
  if (AreThemeSpecificsEquivalent(
          current_specs, theme_specifics,
          theme_service_->IsSystemThemeDistinctFromDefaultTheme())) {
    DVLOG(1) << "Skip setting theme because specs are equal";
    return ThemeSyncState::kApplied;
  }

  base::AutoReset<bool> processing_changes(&processing_syncer_changes_, true);

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
      if (extension_service->IsExtensionEnabled(id)) {
        // An enabled theme extension with the given id was found, so
        // just set the current theme to it.
        theme_service_->SetTheme(extension);
        return ThemeSyncState::kApplied;
      }
      const auto disabled_reasons =
          extensions::ExtensionPrefs::Get(profile_)->GetDisableReasons(id);
      if (disabled_reasons == extensions::disable_reason::DISABLE_USER_ACTION) {
        // The user had installed this theme but disabled it (by installing
        // another atop it); re-enable.
        theme_service_->RevertToExtensionTheme(id);
        return ThemeSyncState::kApplied;
      }
      DVLOG(1) << "Theme " << id << " is disabled with reason "
               << disabled_reasons << "; aborting";
      return ThemeSyncState::kFailed;
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

  bool ntp_background_applied = false;
  if (base::FeatureList::IsEnabled(syncer::kMoveThemePrefsToSpecifics)) {
    if (theme_specifics.has_ntp_background() && profile_->GetPrefs()) {
      DVLOG(1) << "Applying custom NTP background";

      if (base::Value::Dict dict =
              SpecificsNtpBackgroundToDict(theme_specifics.ntp_background());
          !dict.empty()) {
        // TODO(crbug.com/356148174): Set via NtpCustomBackgroundService instead
        // of setting the pref directly.
        profile_->GetPrefs()->SetDict(
            prefs::kNonSyncingNtpCustomBackgroundDictDoNotUse, std::move(dict));
        ntp_background_applied = true;
      }
      // No return since the NTP background exists along with the other themes.
    }

    if (theme_specifics.has_browser_color_scheme()) {
      DVLOG(1) << "Applying browser color scheme";
      theme_service_->SetBrowserColorScheme(ProtoEnumToBrowserColorScheme(
          theme_specifics.browser_color_scheme()));
      // No return, the browser color scheme can coexist with other
      // (non-extension) themes.

      // Before the migration of syncing theme prefs to ThemeSpecifics (see
      // crbug.com/356148174), the specifics will never have
      // `browser_color_scheme` field. However, this field is always populated
      // after the migration. If ThemeSpecifics includes this field, it means
      // another client has already uploaded the latest theme with the new
      // fields. Thus, there's no point in reading the syncing theme prefs
      // anymore.
      if (PrefService* prefs = profile_->GetPrefs()) {
        prefs->SetBoolean(prefs::kShouldReadIncomingSyncingThemePrefs, false);
        pref_service_syncable_observer_.reset();
      }
    }

    if (theme_specifics.has_user_color_theme() &&
        theme_specifics.user_color_theme().has_color() &&
        theme_specifics.user_color_theme().has_browser_color_variant()) {
      DVLOG(1) << "Applying user color";
      theme_service_->SetUserColorAndBrowserColorVariant(
          theme_specifics.user_color_theme().color(),
          ProtoEnumToBrowserColorVariant(
              theme_specifics.user_color_theme().browser_color_variant()));
      return ThemeSyncState::kApplied;
    }

    if (theme_specifics.has_grayscale_theme_enabled()) {
      DVLOG(1) << "Applying grayscale theme";
      theme_service_->SetIsGrayscale(/*is_grayscale=*/true);
      return ThemeSyncState::kApplied;
    }
  }

  if (theme_specifics.has_autogenerated_color_theme()) {
    DVLOG(1) << "Applying autogenerated theme";
    theme_service_->BuildAutogeneratedThemeFromColor(
        theme_specifics.autogenerated_color_theme().color());
    return ThemeSyncState::kApplied;
  }

  // If a custom background was applied, don't reset to the default theme.
  if (ntp_background_applied) {
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

bool ThemeSyncableService::IsCurrentThemeSyncable() const {
  const std::string theme_id = theme_service_->GetThemeID();
  const extensions::Extension* current_extension =
      theme_service_->UsingExtensionTheme() &&
              !theme_service_->UsingDefaultTheme()
          ? extensions::ExtensionRegistry::Get(profile_)
                ->enabled_extensions()
                .GetByID(theme_id)
          : nullptr;
  if (current_extension &&
      !extensions::sync_helper::IsSyncable(current_extension)) {
    DVLOG(1) << "Ignoring non-syncable extension: " << current_extension->id();
    return false;
  }

  // If theme was set through policy, it should be unsyncable.
  if (theme_service_->UsingPolicyTheme()) {
    return false;
  }

  return true;
}

sync_pb::ThemeSpecifics
ThemeSyncableService::GetThemeSpecificsFromCurrentTheme() const {
  sync_pb::ThemeSpecifics theme_specifics;
  theme_specifics.set_use_custom_theme(false);

  const std::string theme_id = theme_service_->GetThemeID();
  const extensions::Extension* current_extension =
      theme_service_->UsingExtensionTheme() &&
              !theme_service_->UsingDefaultTheme()
          ? extensions::ExtensionRegistry::Get(profile_)
                ->enabled_extensions()
                .GetByID(theme_id)
          : nullptr;
  if (current_extension) {
    // Using custom theme and it's an extension.
    DCHECK(current_extension->is_theme());
    theme_specifics.set_use_custom_theme(true);
    theme_specifics.set_custom_theme_name(current_extension->name());
    theme_specifics.set_custom_theme_id(current_extension->id());
    theme_specifics.set_custom_theme_update_url(
        extensions::ManifestURL::GetUpdateURL(current_extension).spec());
  }

  if (base::FeatureList::IsEnabled(syncer::kMoveThemePrefsToSpecifics)) {
    // Fetch ntp background dict from pref.
    // TODO(crbug.com/356148174): Query NtpCustomBackgroundService instead.
    if (PrefService* prefs = profile_->GetPrefs()) {
      if (const base::Value* pref = prefs->GetUserPrefValue(
              prefs::kNonSyncingNtpCustomBackgroundDictDoNotUse)) {
        *theme_specifics.mutable_ntp_background() =
            SpecificsNtpBackgroundFromDict(pref->GetDict());
      }
    }

    theme_specifics.set_browser_color_scheme(
        BrowserColorSchemeToProtoEnum(theme_service_->GetBrowserColorScheme()));

    if (theme_service_->GetIsGrayscale()) {
      theme_specifics.mutable_grayscale_theme_enabled();
    } else if (ThemeService::kUserColorThemeID == theme_id) {
      if (const std::optional<SkColor> user_color =
              theme_service_->GetUserColor()) {
        sync_pb::ThemeSpecifics::UserColorTheme* user_color_theme =
            theme_specifics.mutable_user_color_theme();
        user_color_theme->set_color(*user_color);
        user_color_theme->set_browser_color_variant(
            BrowserColorVariantToProtoEnum(
                theme_service_->GetBrowserColorVariant()));
      }
    }
  }

  if (theme_service_->UsingAutogeneratedTheme()) {
    // Using custom theme and it's autogenerated from color.
    theme_specifics.set_use_custom_theme(false);
    theme_specifics.mutable_autogenerated_color_theme()->set_color(
        theme_service_->GetAutogeneratedThemeColor());
  }

  if (theme_service_->IsSystemThemeDistinctFromDefaultTheme()) {
    // On platform where system theme is different from default theme, set
    // use_system_theme_by_default to true if system theme is used, false
    // if default system theme is used. Otherwise restore it to value in sync.
    if (theme_service_->UsingSystemTheme()) {
      theme_specifics.set_use_system_theme_by_default(true);
    } else if (theme_service_->UsingDefaultTheme()) {
      theme_specifics.set_use_system_theme_by_default(false);
    } else {
      theme_specifics.set_use_system_theme_by_default(
          use_system_theme_by_default_);
    }
  } else {
    // Restore use_system_theme_by_default when platform doesn't distinguish
    // between default theme and system theme.
    theme_specifics.set_use_system_theme_by_default(
        use_system_theme_by_default_);
  }
  return theme_specifics;
}

/* static */
bool ThemeSyncableService::AreThemeSpecificsEquivalent(
    const sync_pb::ThemeSpecifics& a,
    const sync_pb::ThemeSpecifics& b,
    bool is_system_theme_distinct_from_default_theme) {
  if (HasNonDefaultTheme(a) != HasNonDefaultTheme(b)) {
    return false;
  }

  if (a.use_custom_theme() || b.use_custom_theme()) {
    // We're using an extensions theme, so simply compare IDs since those
    // are guaranteed unique.
    return a.use_custom_theme() == b.use_custom_theme() &&
           a.custom_theme_id() == b.custom_theme_id();
  }

  if (base::FeatureList::IsEnabled(syncer::kMoveThemePrefsToSpecifics)) {
    // Since browser color scheme and ntp background can coexist with all other
    // theme types, they're the first ones tested.

    // Compare the two ntp background dicts as whole.
    if ((a.has_ntp_background() || b.has_ntp_background()) &&
        !AreSpecificsNtpBackgroundEquivalent(a.ntp_background(),
                                             b.ntp_background())) {
      return false;
    }
    if (ProtoEnumToBrowserColorScheme(a.browser_color_scheme()) !=
        ProtoEnumToBrowserColorScheme(b.browser_color_scheme())) {
      return false;
    }
    if (a.has_user_color_theme() || b.has_user_color_theme()) {
      return a.has_user_color_theme() == b.has_user_color_theme() &&
             a.user_color_theme().color() == b.user_color_theme().color() &&
             ProtoEnumToBrowserColorVariant(
                 a.user_color_theme().browser_color_variant()) ==
                 ProtoEnumToBrowserColorVariant(
                     b.user_color_theme().browser_color_variant());
    }
    if (a.has_grayscale_theme_enabled() || b.has_grayscale_theme_enabled()) {
      return a.has_grayscale_theme_enabled() == b.has_grayscale_theme_enabled();
    }
  }

  if (a.has_autogenerated_color_theme() || b.has_autogenerated_color_theme()) {
    return a.has_autogenerated_color_theme() ==
               b.has_autogenerated_color_theme() &&
           a.autogenerated_color_theme().color() ==
               b.autogenerated_color_theme().color();
  }
  if (is_system_theme_distinct_from_default_theme) {
    // We're not using a custom theme, but we care about system
    // vs. default.
    return a.use_system_theme_by_default() == b.use_system_theme_by_default();
  }
  // We're not using a custom theme, and we don't care about system
  // vs. default.
  return true;
}

bool ThemeSyncableService::HasNonDefaultTheme(
    const sync_pb::ThemeSpecifics& theme_specifics) {
  return theme_specifics.use_custom_theme() ||
         theme_specifics.has_autogenerated_color_theme() ||
         (base::FeatureList::IsEnabled(syncer::kMoveThemePrefsToSpecifics) &&
          (theme_specifics.has_user_color_theme() ||
           theme_specifics.has_grayscale_theme_enabled() ||
           HasNonDefaultBrowserColorScheme(theme_specifics) ||
           theme_specifics.has_ntp_background()));
}

std::optional<syncer::ModelError> ThemeSyncableService::ProcessNewTheme(
    syncer::SyncChange::SyncChangeType change_type,
    const sync_pb::ThemeSpecifics& theme_specifics) {
  // As part of the theme migration strategy, update the old syncing prefs with
  // the new values.
  if (PrefService* prefs = profile_->GetPrefs()) {
    for (const auto& [pref_in_migration, pref_names] : kThemePrefsInMigration) {
      if (const base::Value* value =
              prefs->GetUserPrefValue(pref_names.non_syncing_pref_name)) {
        prefs->Set(pref_names.syncing_pref_name, value->Clone());
      }
    }
  }

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

  for (Observer& observer : observer_list_) {
    observer.OnThemeSyncStarted(startup_state);
  }
}
