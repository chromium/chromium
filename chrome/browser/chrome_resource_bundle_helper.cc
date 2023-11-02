// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chrome_resource_bundle_helper.h"

#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/strings/utf_string_conversions.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
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
#include "ui/base/resource/resource_bundle.h"

#if BUILDFLAG(IS_ANDROID)
#include "ui/base/resource/resource_bundle_android.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/constants/ash_switches.h"
#include "chrome/common/pref_names.h"
#include "ui/lottie/resource.h"  // nogncheck
#endif

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "ui/base/ui_base_switches.h"
#endif

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "extensions/common/extension_l10n_util.h"
#endif

namespace {

extern void InitializeLocalState(
    ChromeFeatureListCreator* chrome_feature_list_creator) {
  TRACE_EVENT0("startup", "ChromeBrowserMainParts::InitializeLocalState");

#if BUILDFLAG(IS_CHROMEOS_ASH)
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(ash::switches::kLoginManager)) {
    PrefService* local_state = chrome_feature_list_creator->local_state();
    DCHECK(local_state);

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
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
}

// Initializes the shared instance of ResourceBundle and returns the application
// locale. An empty |actual_locale| value indicates failure.
std::string InitResourceBundleAndDetermineLocale(PrefService* local_state,
                                                 bool is_running_tests) {
#if BUILDFLAG(IS_ANDROID)
  // In order for SetLoadSecondaryLocalePaks() to work ResourceBundle must
  // not have been created yet.
  DCHECK(!ui::ResourceBundle::HasSharedInstance());
  // Auto-detect based on en-US whether secondary locale .pak files exist.
  bool in_split = false;
  bool log_error = false;
  ui::SetLoadSecondaryLocalePaks(
      !ui::GetPathForAndroidLocalePakWithinApk("en-US", in_split, log_error)
           .empty());
#endif

  std::string preferred_locale;
#if BUILDFLAG(IS_MAC)
  // TODO(markusheintz): Read preference pref::kApplicationLocale in order
  // to enforce the application locale.
  // Tests always get en-US.
  preferred_locale = is_running_tests ? "en-US" : std::string();
#else
  preferred_locale =
      local_state->GetString(language::prefs::kApplicationLocale);
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
  ui::ResourceBundle::SetLottieParsingFunctions(
      &lottie::ParseLottieAsStillImage, &lottie::ParseLottieAsThemedStillImage);
#endif

  TRACE_EVENT0("startup",
               "ChromeBrowserMainParts::InitResourceBundleAndDetermineLocale");
  // On a POSIX OS other than ChromeOS, the parameter that is passed to the
  // method InitSharedInstance is ignored.
  std::string actual_locale = ui::ResourceBundle::InitSharedInstanceWithLocale(
      preferred_locale, nullptr, ui::ResourceBundle::LOAD_COMMON_RESOURCES);
  CHECK(!actual_locale.empty())
      << "Locale could not be found for " << preferred_locale;

  // First run prefs needs data from the ResourceBundle, so load it now.
  {
    TRACE_EVENT0("startup",
                 "ChromeBrowserMainParts::InitResourceBundleAndDetermineLocale:"
                 ":AddDataPack");
    base::FilePath resources_pack_path;
    base::PathService::Get(chrome::FILE_RESOURCES_PACK, &resources_pack_path);
#if BUILDFLAG(IS_ANDROID)
    ui::LoadMainAndroidPackFile("assets/resources.pak", resources_pack_path);

    // Avoid loading DFM native resources here, to keep startup lean. These
    // resources are loaded on-use, when an already-installed DFM loads.
#elif BUILDFLAG(IS_CHROMEOS_LACROS)
    base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
    if (command_line->HasSwitch(switches::kEnableResourcesFileSharing)) {
      // If LacrosResourcesFileSharing feature is enabled, Lacros refers to ash
      // resources pak file.
      base::FilePath ash_resources_pack_path;
      base::PathService::Get(chrome::FILE_ASH_RESOURCES_PACK,
                             &ash_resources_pack_path);
      base::FilePath shared_resources_pack_path;
      base::PathService::Get(chrome::FILE_RESOURCES_FOR_SHARING_PACK,
                             &shared_resources_pack_path);
      ui::ResourceBundle::GetSharedInstance()
          .AddDataPackFromPathWithAshResources(
              shared_resources_pack_path, ash_resources_pack_path,
              resources_pack_path, ui::kScaleFactorNone);
    } else {
      ui::ResourceBundle::GetSharedInstance().AddDataPackFromPath(
          resources_pack_path, ui::kScaleFactorNone);
    }
#else
    ui::ResourceBundle::GetSharedInstance().AddDataPackFromPath(
        resources_pack_path, ui::kScaleFactorNone);
#endif  // BUILDFLAG(IS_ANDROID)
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
