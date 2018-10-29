// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chrome_resource_bundle_helper.h"

#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/strings/utf_string_conversions.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "chrome/browser/first_run/first_run.h"
#include "chrome/browser/metrics/chrome_feature_list_creator.h"
#include "chrome/browser/prefs/chrome_command_line_pref_store.h"
#include "chrome/browser/prefs/chrome_pref_service_factory.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/installer/util/google_update_settings.h"
#include "components/language/core/browser/pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "extensions/buildflags/buildflags.h"
#include "extensions/common/extension_l10n_util.h"
#include "ui/base/resource/resource_bundle.h"

#if defined(OS_ANDROID)
#include "ui/base/resource/resource_bundle_android.h"
#endif

#if defined(OS_CHROMEOS)
#include "chrome/common/pref_names.h"
#include "chromeos/chromeos_switches.h"
#endif

namespace {

extern void InitializeLocalState(
    ChromeFeatureListCreator* chrome_feature_list_creator) {
  TRACE_EVENT0("startup", "ChromeBrowserMainParts::InitializeLocalState");

  // Load local state. This includes the application locale so we know which
  // locale dll to load. This also causes local state prefs to be registered.
  PrefService* local_state = chrome_feature_list_creator->local_state();
  DCHECK(local_state);
#if defined(OS_WIN)
  if (first_run::IsChromeFirstRun()) {
    // During first run we read the google_update registry key to find what
    // language the user selected when downloading the installer. This
    // becomes our default language in the prefs.
    // Other platforms obey the system locale.
    base::string16 install_lang;
    if (GoogleUpdateSettings::GetLanguage(&install_lang)) {
      local_state->SetString(language::prefs::kApplicationLocale,
                             base::UTF16ToASCII(install_lang));
    }
  }
#endif  // defined(OS_WIN)

  // If the local state file for the current profile doesn't exist and the
  // parent profile command line flag is present, then we should inherit some
  // local state from the parent profile.
  // Checking that the local state file for the current profile doesn't exist
  // is the most robust way to determine whether we need to inherit or not
  // since the parent profile command line flag can be present even when the
  // current profile is not a new one, and in that case we do not want to
  // inherit and reset the user's setting.
  //
  // TODO(mnissler): We should probably just instantiate a
  // JSONPrefStore here instead of an entire PrefService. Once this is
  // addressed, the call to browser_prefs::RegisterLocalState can move
  // to chrome_prefs::CreateLocalState.
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kParentProfile)) {
    base::FilePath local_state_path;
    base::PathService::Get(chrome::FILE_LOCAL_STATE, &local_state_path);
    bool local_state_file_exists = base::PathExists(local_state_path);
    if (!local_state_file_exists) {
      base::FilePath parent_profile =
          command_line->GetSwitchValuePath(switches::kParentProfile);
      scoped_refptr<PrefRegistrySimple> registry =
          base::MakeRefCounted<PrefRegistrySimple>();
      registry->RegisterStringPref(language::prefs::kApplicationLocale,
                                   std::string());
      const std::unique_ptr<PrefService> parent_local_state =
          chrome_prefs::CreateLocalState(
              parent_profile,
              chrome_feature_list_creator->browser_policy_connector()
                  ->GetPolicyService(),
              std::move(registry), false, nullptr,
              chrome_feature_list_creator->browser_policy_connector());
      // Right now, we only inherit the locale setting from the parent profile.
      local_state->SetString(
          language::prefs::kApplicationLocale,
          parent_local_state->GetString(language::prefs::kApplicationLocale));
    }
  }

#if defined(OS_CHROMEOS)
  if (command_line->HasSwitch(chromeos::switches::kLoginManager)) {
    std::string owner_locale = local_state->GetString(prefs::kOwnerLocale);
    // Ensure that we start with owner's locale.
    if (!owner_locale.empty() &&
        local_state->GetString(language::prefs::kApplicationLocale) !=
            owner_locale &&
        !local_state->IsManagedPreference(
            language::prefs::kApplicationLocale)) {
      local_state->SetString(language::prefs::kApplicationLocale, owner_locale);
    }
  }
#endif  // defined(OS_CHROMEOS)
}

// Initializes the shared instance of ResourceBundle and returns the application
// locale. An empty |actual_locale| value indicates failure.
std::string InitResourceBundleAndDetermineLocale(PrefService* local_state,
                                                 bool is_running_tests) {
#if defined(OS_ANDROID)
  // In order for SetLoadSecondaryLocalePaks() to work ResourceBundle must
  // not have been created yet.
  DCHECK(!ui::ResourceBundle::HasSharedInstance());
  // Auto-detect based on en-US whether secondary locale .pak files exist.
  ui::SetLoadSecondaryLocalePaks(
      !ui::GetPathForAndroidLocalePakWithinApk("en-US").empty());
#endif

  std::string preferred_locale;
#if defined(OS_MACOSX)
  // TODO(markusheintz): Read preference pref::kApplicationLocale in order
  // to enforce the application locale.
  // Tests always get en-US.
  preferred_locale = is_running_tests ? "en-US" : std::string();
#else
  preferred_locale =
      local_state->GetString(language::prefs::kApplicationLocale);
#endif

  TRACE_EVENT0("startup",
               "ChromeBrowserMainParts::InitResourceBundleAndDetermineLocale");
  // On a POSIX OS other than ChromeOS, the parameter that is passed to the
  // method InitSharedInstance is ignored.
  std::string actual_locale = ui::ResourceBundle::InitSharedInstanceWithLocale(
      preferred_locale, nullptr, ui::ResourceBundle::LOAD_COMMON_RESOURCES);
  if (actual_locale.empty())
    return actual_locale;

  // First run prefs needs data from the ResourceBundle, so load it now.
  {
    TRACE_EVENT0("startup",
                 "ChromeBrowserMainParts::InitResourceBundleAndDetermineLocale:"
                 ":AddDataPack");
    base::FilePath resources_pack_path;
    base::PathService::Get(chrome::FILE_RESOURCES_PACK, &resources_pack_path);
#if defined(OS_ANDROID)
    ui::LoadMainAndroidPackFile("assets/resources.pak", resources_pack_path);
#else
    ui::ResourceBundle::GetSharedInstance().AddDataPackFromPath(
        resources_pack_path, ui::SCALE_FACTOR_NONE);
#endif  // defined(OS_ANDROID)
  }

#if BUILDFLAG(ENABLE_EXTENSIONS)
  extension_l10n_util::SetProcessLocale(actual_locale);
  extension_l10n_util::SetPreferredLocale(preferred_locale);
#endif
  return actual_locale;
}

}  // namespace

std::string LoadLocalState(
    ChromeFeatureListCreator* chrome_feature_list_creator,
    bool is_running_tests) {
  base::FilePath user_data_dir;
  if (!base::PathService::Get(chrome::DIR_USER_DATA, &user_data_dir))
    return std::string();

  InitializeLocalState(chrome_feature_list_creator);

  chrome_feature_list_creator->local_state()->UpdateCommandLinePrefStore(
      new ChromeCommandLinePrefStore(base::CommandLine::ForCurrentProcess()));

  return InitResourceBundleAndDetermineLocale(
      chrome_feature_list_creator->local_state(), is_running_tests);
}
