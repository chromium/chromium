// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/themes/theme_syncable_service.h"

#include <stddef.h>

#include <string>
#include <utility>

#include "base/auto_reset.h"
#include "base/base64.h"
#include "base/containers/fixed_flat_map.h"
#include "base/feature_list.h"
#include "base/metrics/histogram_functions.h"
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
#include "components/sync/protocol/entity_data.h"
#include "components/sync/protocol/entity_specifics.pb.h"
#include "components/sync/protocol/proto_value_conversions.h"
#include "components/sync/protocol/theme_specifics.pb.h"
#include "components/sync/protocol/theme_types.pb.h"
#include "components/sync_preferences/pref_service_syncable.h"
#include "components/sync_preferences/pref_service_syncable_observer.h"
#include "extensions/browser/disable_reason.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registrar.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/pending_extension_info.h"
#include "extensions/browser/pending_extension_manager.h"
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
         {prefs::kDeprecatedBrowserColorSchemeDoNotUse,
          prefs::kBrowserColorScheme}},
        {ThemePrefInMigration::kUserColor,
         {prefs::kDeprecatedUserColorDoNotUse, prefs::kUserColor}},
        {ThemePrefInMigration::kBrowserColorVariant,
         {prefs::kDeprecatedBrowserColorVariantDoNotUse,
          prefs::kBrowserColorVariant}},
        {ThemePrefInMigration::kGrayscaleThemeEnabled,
         {prefs::kDeprecatedGrayscaleThemeEnabledDoNotUse,
          prefs::kGrayscaleThemeEnabled}},
        {ThemePrefInMigration::kNtpCustomBackgroundDict,
         {prefs::kDeprecatedNtpCustomBackgroundDictDoNotUse,
          prefs::kNtpCustomBackgroundDict}},
    });

static_assert(
    kThemePrefsInMigration.size() ==
        static_cast<size_t>(ThemePrefInMigration::kMaxValue) + 1,
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

std::optional<base::Value::Dict> NtpBackgroundDictFromSpecifics(
    const sync_pb::ThemeSpecifics& theme_specifics) {
  if (!theme_specifics.has_ntp_background()) {
    return std::nullopt;
  }
  const sync_pb::NtpCustomBackground& ntp_background =
      theme_specifics.ntp_background();
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

sync_pb::NtpCustomBackground SpecificsNtpBackgroundFromDict(
    const base::Value::Dict& dict) {
  sync_pb::NtpCustomBackground ntp_background;
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
    const sync_pb::NtpCustomBackground& a,
    const sync_pb::NtpCustomBackground& b) {
  // MessageDifferencer cannot be used and explicitly comparing all the fields
  // is maintenance-heavy.
  return a.SerializeAsString() == b.SerializeAsString();
}

}  // namespace

// "Current" is part of the name for historical reasons, shouldn't be changed.
const char ThemeSyncableService::kSyncEntityClientTag[] = "current_theme";
const char ThemeSyncableService::kSyncEntityTitle[] = "Current Theme";

void MigrateSyncingThemePrefsToNonSyncingIfNeeded(PrefService* prefs) {
  const bool already_migrated =
      prefs->GetBoolean(prefs::kSyncingThemePrefsMigratedToNonSyncing);
  base::UmaHistogramBoolean("Theme.ThemePrefMigration.AlreadyMigrated",
                            already_migrated);
  if (already_migrated) {
    return;
  }
  for (const auto& [pref_in_migration, pref_names] : kThemePrefsInMigration) {
    if (const base::Value* value =
            prefs->GetUserPrefValue(pref_names.syncing_pref_name)) {
      prefs->Set(pref_names.non_syncing_pref_name, value->Clone());
      base::UmaHistogramEnumeration("Theme.ThemePrefMigration.MigratedPref",
                                    pref_in_migration);
    }
  }

  prefs->SetBoolean(prefs::kSyncingThemePrefsMigratedToNonSyncing, true);
}

class ThemeSyncableService::PrefServiceSyncableObserver
    : public sync_preferences::PrefServiceSyncableObserver {
 public:
  PrefServiceSyncableObserver(sync_preferences::PrefServiceSyncable* prefs,
                              ThemeSyncableService* theme_syncable_service)
      : prefs_(prefs), theme_syncable_service_(theme_syncable_service) {
    observation_.Observe(prefs);
    // Prefs sync might have already started.
    OnIsSyncingChanged();
  }

  void OnIsSyncingChanged() override {
    CHECK(prefs_->GetBoolean(prefs::kShouldReadIncomingSyncingThemePrefs));
    if (prefs_->IsSyncing()) {
      observation_.Reset();
      bool should_notify = false;
      {
        // Block self-induced notifications (see crbug.com/375553464).
        base::AutoReset<bool> processing_changes(
            &theme_syncable_service_->processing_syncer_changes_, true);

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
            base::UmaHistogramEnumeration(
                "Theme.ThemePrefMigration.IncomingSyncingPrefApplied",
                pref_in_migration);
            should_notify = true;
          }
        }
      }
      prefs_->SetBoolean(prefs::kShouldReadIncomingSyncingThemePrefs, false);
      if (should_notify) {
        theme_syncable_service_->OnThemeChanged();
      }
    }
  }

 private:
  base::ScopedObservation<sync_preferences::PrefServiceSyncable,
                          sync_preferences::PrefServiceSyncableObserver>
      observation_{this};
  raw_ptr<sync_preferences::PrefServiceSyncable> prefs_;
  raw_ptr<ThemeSyncableService> theme_syncable_service_;
};

ThemeSyncableService::ThemeSyncableService(Profile* profile,
                                           ThemeService* theme_service)
    : profile_(profile),
      theme_service_(theme_service),
      use_system_theme_by_default_(false) {
  CHECK(profile_);
  CHECK(profile_->GetPrefs());
  DCHECK(theme_service_);
  theme_service_->AddObserver(this);

  sync_preferences::PrefServiceSyncable* prefs =
      static_cast<sync_preferences::PrefServiceSyncable*>(profile_->GetPrefs());
  // Listen to NtpCustomBackgroundDict pref changes. This is done because
  // ThemeService doesn't convey ntp background change notifications.
  pref_change_registrar_.Init(prefs);
  pref_change_registrar_.Add(
      prefs::kNtpCustomBackgroundDict,
      base::BindRepeating(&ThemeSyncableService::OnThemeChanged,
                          base::Unretained(this)));

  if (prefs->GetBoolean(prefs::kShouldReadIncomingSyncingThemePrefs)) {
    // ThemeSyncableService instance is destroyed upon ThemeService::Shutdown.
    // So `prefs` outlives this.
    pref_service_syncable_observer_ =
        std::make_unique<PrefServiceSyncableObserver>(
            prefs,
            // This is okay since `this` outlives
            // `pref_service_syncable_observer_`.
            this);
  }
}

ThemeSyncableService::~ThemeSyncableService() {
  pref_service_syncable_observer_.reset();
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

void ThemeSyncableService::WillStartInitialSync() {
  if (base::FeatureList::IsEnabled(syncer::kSeparateLocalAndAccountThemes)) {
    // Save current theme specifics to pref. This is used to restore the local
    // theme upon signout.
    profile_->GetPrefs()->SetString(
        prefs::kSavedLocalTheme,
        base::Base64Encode(
            GetThemeSpecificsFromCurrentTheme().SerializeAsString()));
  }
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
    return syncer::ModelError(FROM_HERE,
                              syncer::ModelError::Type::kThemeTooManySpecifics);
  }

  if (!IsCurrentThemeSyncable()) {
    // Current theme is unsyncable - don't overwrite from sync data, and don't
    // save the unsyncable theme to sync data.
    NotifyOnSyncStarted(ThemeSyncState::kFailed);
    return std::nullopt;
  }

  const sync_pb::ThemeSpecifics current_specifics =
      GetThemeSpecificsFromCurrentTheme();
  if (!initial_sync_data.empty() &&
      initial_sync_data[0].GetSpecifics().has_theme()) {
    const sync_pb::ThemeSpecifics& new_specifics =
        initial_sync_data[0].GetSpecifics().theme();
    if (!HasNonDefaultTheme(current_specifics) ||
        HasNonDefaultTheme(new_specifics)) {
      ThemeSyncState startup_state =
          MaybeSetTheme(current_specifics, new_specifics);
      // Commit the current theme if it has changed and is different from the
      // remote theme. This can happen when theme attributes which were
      // earlier synced via prefs (user color and ntp background), are now
      // populated in ThemeSpecifics. This new ThemeSpecifics should be
      // committed to the server. Note that this is avoided for incoming
      // extension themes as they are applied from a posted task and will call
      // OnThemeChanged() when set and commit the current theme.
      if (startup_state == ThemeSyncState::kApplied &&
          !new_specifics.use_custom_theme() &&
          !AreThemeSpecificsEquivalent(
              GetThemeSpecificsFromCurrentTheme(), new_specifics,
              theme_service_->IsSystemThemeDistinctFromDefaultTheme())) {
        OnThemeChanged();
      }
      NotifyOnSyncStarted(startup_state);
      return std::nullopt;
    }
  }

  // No theme specifics found. Commit one according to current theme if
  // kSeparateLocalAndAccountThemes feature flag is not enabled.
  std::optional<syncer::ModelError> error =
      base::FeatureList::IsEnabled(syncer::kSeparateLocalAndAccountThemes)
          ? std::nullopt
          : ProcessNewTheme(syncer::SyncChange::ACTION_ADD, current_specifics);
  NotifyOnSyncStarted(ThemeSyncState::kApplied);
  return error;
}

void ThemeSyncableService::StopSyncing(syncer::DataType type) {
  CHECK(thread_checker_.CalledOnValidThread());
  CHECK_EQ(type, syncer::THEMES);

  sync_processor_.reset();

  if (base::FeatureList::IsEnabled(syncer::kSeparateLocalAndAccountThemes)) {
    // It is possible that saved local theme was cleared by the batch uploader.
    // In such a case, apply the default theme.
    const bool result = ApplySavedLocalThemeIfExistsAndClear();
    base::UmaHistogramBoolean("Theme.RestoredLocalThemeUponSignout", result);
    if (!result) {
      theme_service_->UseDefaultTheme();
      // Explicitly reset the browser color scheme to default because
      // UseDefaultTheme() does not do it.
      // TODO(crbug.com/442592525): Consider resetting the browser color scheme
      // in UseDefaultTheme().
      theme_service_->SetBrowserColorScheme(
          ThemeService::BrowserColorScheme::kSystem);
    }
  }
}

void ThemeSyncableService::StayStoppedAndMaybeClearData(syncer::DataType type) {
  CHECK(thread_checker_.CalledOnValidThread());
  CHECK_EQ(type, syncer::THEMES);
  CHECK(!sync_processor_);

  if (base::FeatureList::IsEnabled(syncer::kSeparateLocalAndAccountThemes)) {
    // Avoid applying the default theme unlike StopSyncing() does because this
    // method can be called multiple times and applying default theme will cause
    // the local theme to be lost.
    ApplySavedLocalThemeIfExistsAndClear();
  }
}

void ThemeSyncableService::OnBrowserShutdown(syncer::DataType type) {
  CHECK(thread_checker_.CalledOnValidThread());
  CHECK_EQ(type, syncer::THEMES);

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
    return syncer::ModelError(
        FROM_HERE, syncer::ModelError::Type::kThemeSyncableServiceNotStarted);
  }

  // TODO(akalin): Normally, we should only have a single change and
  // it should be an update.  However, the syncapi may occasionally
  // generates multiple changes.  When we fix syncapi to not do that,
  // we can remove the extra logic below.  See:
  // http://code.google.com/p/chromium/issues/detail?id=41696 .
  if (change_list.size() != 1) {
    return syncer::ModelError(FROM_HERE,
                              syncer::ModelError::Type::kThemeTooManyChanges);
  }
  const syncer::SyncChange& theme_change = change_list[0];
  if (theme_change.change_type() != syncer::SyncChange::ACTION_ADD &&
      theme_change.change_type() != syncer::SyncChange::ACTION_UPDATE) {
    return syncer::ModelError(
        FROM_HERE, syncer::ModelError::Type::kThemeInvalidChangeType);
  }

  if (!IsCurrentThemeSyncable()) {
    // Current theme is unsyncable, so don't overwrite it.
    return std::nullopt;
  }

  // Set current theme from the theme specifics.
  if (theme_change.sync_data().GetSpecifics().has_theme()) {
    MaybeSetTheme(GetThemeSpecificsFromCurrentTheme(),
                  theme_change.sync_data().GetSpecifics().theme());
    return std::nullopt;
  }

  return syncer::ModelError(FROM_HERE,
                            syncer::ModelError::Type::kThemeMissingSpecifics);
}

base::WeakPtr<syncer::SyncableService> ThemeSyncableService::AsWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

std::string ThemeSyncableService::GetClientTag(
    const syncer::EntityData& entity_data) const {
  DCHECK(entity_data.specifics.has_theme());
  // Theme always returns the same client tag as there is only one single theme
  // entity.
  return kSyncEntityClientTag;
}

ThemeSyncableService::ThemeSyncState ThemeSyncableService::MaybeSetTheme(
    const sync_pb::ThemeSpecifics& current_specs,
    const sync_pb::ThemeSpecifics& new_specs) {
  use_system_theme_by_default_ = new_specs.use_system_theme_by_default();
  if (AreThemeSpecificsEquivalent(
          current_specs, new_specs,
          theme_service_->IsSystemThemeDistinctFromDefaultTheme())) {
    DVLOG(1) << "Skip setting theme because specs are equal";
    return ThemeSyncState::kApplied;
  }

  // Whether the ThemeSpecifics is from a client which commits all theme
  // attributes via ThemeSpecifics.
  const bool has_all_theme_attributes = new_specs.has_browser_color_scheme();
  // The new specifics will always include `browser_color_scheme` field. If it
  // is absent and the theme specifics is the default theme, avoid setting to
  // default theme. This is because the old clients can send such specifics upon
  // any change to theme sent via preferences which the new clients do not read.
  if (!has_all_theme_attributes && !HasNonDefaultTheme(new_specs)) {
    DVLOG(1) << "Skip setting default theme from old clients";
    return ThemeSyncState::kApplied;
  }

  base::AutoReset<bool> processing_changes(&processing_syncer_changes_, true);

  // Browser color scheme can be set alongside other themes, including extension
  // theme.
  if (has_all_theme_attributes) {
    DVLOG(1) << "Applying browser color scheme";
    theme_service_->SetBrowserColorScheme(
        ProtoEnumToBrowserColorScheme(new_specs.browser_color_scheme()));

    // Prior to the ThemeSpecifics migration (crbug.com/356148174),
    // 'browser_color_scheme' was absent. Post-migration, it's always set. If
    // this field exists, a newer theme has been synced, making reading the
    // syncing theme prefs pointless.
    profile_->GetPrefs()->SetBoolean(
        prefs::kShouldReadIncomingSyncingThemePrefs, false);
    pref_service_syncable_observer_.reset();
  }

  if (new_specs.use_custom_theme()) {
    string id(new_specs.custom_theme_id());
    GURL update_url(new_specs.custom_theme_update_url());
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
      auto* extension_registrar = extensions::ExtensionRegistrar::Get(profile_);
      if (extension_registrar->IsExtensionEnabled(id)) {
        // An enabled theme extension with the given id was found, so
        // just set the current theme to it.
        theme_service_->SetTheme(extension);
        return ThemeSyncState::kApplied;
      }
      bool is_disabled_by_user =
          extensions::ExtensionPrefs::Get(profile_)->HasOnlyDisableReason(
              id, extensions::disable_reason::DISABLE_USER_ACTION);
      if (is_disabled_by_user) {
        // The user had installed this theme but disabled it (by installing
        // another atop it); re-enable.
        theme_service_->RevertToExtensionTheme(id);
        return ThemeSyncState::kApplied;
      }
      DVLOG(1) << "Theme " << id
               << " is disabled with reasons other than DISABLE_USER_ACTION "
               << "; aborting";
      return ThemeSyncState::kFailed;
    }

    // No extension with this id exists -- we must install it; we do
    // so by adding it as a pending extension and then triggering an
    // auto-update cycle.
    const bool kRemoteInstall = false;
    if (!extensions::PendingExtensionManager::Get(profile_)->AddFromSync(
            id, update_url, base::Version(), &IsTheme, kRemoteInstall)) {
      LOG(WARNING) << "Could not add pending extension for " << id;
      return ThemeSyncState::kFailed;
    }
    remote_extension_theme_pending_install_ = id;
    extension_service->CheckForUpdatesSoon();
    // Return that the call triggered an extension theme installation.
    return ThemeSyncState::kWaitingForExtensionInstallation;
  }

  // Apply theme besides the NTP background and the browser color scheme. These
  // themes cannot exist alongside each other.
  if (new_specs.has_user_color_theme() &&
      new_specs.user_color_theme().has_color() &&
      new_specs.user_color_theme().has_browser_color_variant()) {
    DVLOG(1) << "Applying user color";
    theme_service_->SetUserColorAndBrowserColorVariant(
        new_specs.user_color_theme().color(),
        ProtoEnumToBrowserColorVariant(
            new_specs.user_color_theme().browser_color_variant()));
  } else if (new_specs.has_grayscale_theme_enabled()) {
    DVLOG(1) << "Applying grayscale theme";
    theme_service_->SetIsGrayscale(/*is_grayscale=*/true);
  } else if (new_specs.has_autogenerated_color_theme()) {
    DVLOG(1) << "Applying autogenerated theme";
    theme_service_->BuildAutogeneratedThemeFromColor(
        new_specs.autogenerated_color_theme().color());
  } else if (new_specs.use_system_theme_by_default()) {
    DVLOG(1) << "Switch to use system theme";
    theme_service_->UseSystemTheme();
  } else {
    // NOTE: No need to check for `is_new_specifics` before setting to default
    // theme. Empty incoming themes are ignored in MergeDataAndStartSyncing().
    DVLOG(1) << "Switch to use default theme";
    theme_service_->UseDefaultTheme();
  }

  PrefService* prefs = profile_->GetPrefs();
  // NTP background can exist along with the other (non-extension) themes.
  if (std::optional<base::Value::Dict> dict =
          NtpBackgroundDictFromSpecifics(new_specs);
      dict && !dict->empty()) {
    DVLOG(1) << "Applying custom NTP background";
    // TODO(crbug.com/356148174): Set via NtpCustomBackgroundService instead
    // of setting the pref directly.
    prefs->SetDict(prefs::kNtpCustomBackgroundDict, std::move(*dict));
  } else if (has_all_theme_attributes) {
    // Clear the current ntp background if none received from remote.
    // NOTE: Ntp background is only cleared if the incoming ThemeSpecifics
    // is the new one and is missing the ntp_background field because it was
    // committed by an old client.
    DVLOG(1) << "Removing custom NTP background";
    prefs->ClearPref(prefs::kNtpCustomBackgroundDict);
  }
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
  // Set this to `use_system_theme_by_default_` which is the value received from
  // sync. If this platform supports distinct system theme, the value might be
  // overridden below depending on the current theme.
  theme_specifics.set_use_system_theme_by_default(use_system_theme_by_default_);

  // Always set the browser color scheme, to denote that the ThemeSpecifics
  // contains all the theme attributes.
  theme_specifics.set_browser_color_scheme(
      BrowserColorSchemeToProtoEnum(theme_service_->GetBrowserColorScheme()));

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
    return theme_specifics;
  }

  // Skip setting background in the specifics if the background is set using
  // local resource.
  PrefService* prefs = profile_->GetPrefs();
  if (!prefs->GetBoolean(prefs::kNtpCustomBackgroundLocalToDevice)) {
    // Fetch ntp background dict from pref.
    // TODO(crbug.com/356148174): Query NtpCustomBackgroundService instead.
    if (const base::Value* pref =
            prefs->GetUserPrefValue(prefs::kNtpCustomBackgroundDict)) {
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
      sync_pb::UserColorTheme* user_color_theme =
          theme_specifics.mutable_user_color_theme();
      user_color_theme->set_color(*user_color);
      user_color_theme->set_browser_color_variant(
          BrowserColorVariantToProtoEnum(
              theme_service_->GetBrowserColorVariant()));
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
    // if default system theme is used. Otherwise keep it to the value received
    // from sync (`use_system_theme_by_default_`).
    if (theme_service_->UsingSystemTheme()) {
      theme_specifics.set_use_system_theme_by_default(true);
    } else if (theme_service_->UsingDefaultTheme()) {
      theme_specifics.set_use_system_theme_by_default(false);
    }
  }
  return theme_specifics;
}

sync_pb::ThemeSpecifics
ThemeSyncableService::GetThemeSpecificsFromCurrentThemeForTesting() const {
  return GetThemeSpecificsFromCurrentTheme();
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
         theme_specifics.has_user_color_theme() ||
         theme_specifics.has_grayscale_theme_enabled() ||
         HasNonDefaultBrowserColorScheme(theme_specifics) ||
         theme_specifics.has_ntp_background();
}

std::optional<syncer::ModelError> ThemeSyncableService::ProcessNewTheme(
    syncer::SyncChange::SyncChangeType change_type,
    const sync_pb::ThemeSpecifics& theme_specifics) {
  // As part of the theme migration strategy, update the old syncing prefs with
  // the new values.
  PrefService* prefs = profile_->GetPrefs();
  for (const auto& [pref_in_migration, pref_names] : kThemePrefsInMigration) {
    // Skip setting ntp background pref if the background is currently set
    // using a local resource.
    if (pref_in_migration == ThemePrefInMigration::kNtpCustomBackgroundDict &&
        prefs->GetBoolean(prefs::kNtpCustomBackgroundLocalToDevice)) {
      continue;
    }
    if (const base::Value* value =
            prefs->GetUserPrefValue(pref_names.non_syncing_pref_name)) {
      prefs->Set(pref_names.syncing_pref_name, *value);
    } else {
      prefs->ClearPref(pref_names.syncing_pref_name);
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

std::optional<sync_pb::ThemeSpecifics>
ThemeSyncableService::GetSavedLocalTheme() const {
  CHECK(base::FeatureList::IsEnabled(syncer::kSeparateLocalAndAccountThemes));
  if (const base::Value* saved_local_theme =
          profile_->GetPrefs()->GetUserPrefValue(prefs::kSavedLocalTheme)) {
    std::string decoded_str;
    sync_pb::ThemeSpecifics specifics;
    // The local theme is saved as a base64 encoded string.
    if (base::Base64Decode(saved_local_theme->GetString(), &decoded_str) &&
        specifics.ParseFromString(decoded_str)) {
      return specifics;
    }
  }
  return std::nullopt;
}

bool ThemeSyncableService::ApplySavedLocalThemeIfExistsAndClear() {
  CHECK(base::FeatureList::IsEnabled(syncer::kSeparateLocalAndAccountThemes));
  std::optional<sync_pb::ThemeSpecifics> local_theme_specifics =
      GetSavedLocalTheme();
  if (local_theme_specifics) {
    // This does not trigger a notification to OnThemeChanged() and thus does
    // not commit the theme change to sync. That is done below.
    MaybeSetTheme(GetThemeSpecificsFromCurrentTheme(), *local_theme_specifics);
    if (remote_extension_theme_pending_install_) {
      extensions::PendingExtensionManager* pending_extension_manager =
          extensions::PendingExtensionManager::Get(profile_);
      // If the theme extension is still pending installation, remove from the
      // queue.
      if (const extensions::PendingExtensionInfo* extension =
              pending_extension_manager->GetById(
                  *remote_extension_theme_pending_install_);
          extension && extension->is_from_sync()) {
        pending_extension_manager->Remove(
            *remote_extension_theme_pending_install_);
      }
      // Remove any unused theme extension. This should remove
      // `remote_extension_theme_pending_install_` if it was installed.
      theme_service_->RemoveUnusedThemes();
    }
    // Commit the theme change to sync. Note that this does not trigger a commit
    // when called while StopSyncing().
    OnThemeChanged();
  }
  profile_->GetPrefs()->ClearPref(prefs::kSavedLocalTheme);
  return local_theme_specifics.has_value();
}
