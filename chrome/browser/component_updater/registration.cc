// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/component_updater/registration.h"

#include "base/files/file_path.h"
#include "base/metrics/histogram_functions.h"
#include "base/path_service.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/buildflags.h"
#include "chrome/browser/component_updater/autofill_regex_component_installer.h"
#include "chrome/browser/component_updater/chrome_origin_trials_component_installer.h"
#include "chrome/browser/component_updater/crl_set_component_installer.h"
#include "chrome/browser/component_updater/crowd_deny_component_installer.h"
#include "chrome/browser/component_updater/desktop_sharing_hub_component_installer.h"
#include "chrome/browser/component_updater/file_type_policies_component_installer.h"
#include "chrome/browser/component_updater/first_party_sets_component_installer.h"
#include "chrome/browser/component_updater/floc_component_installer.h"
#include "chrome/browser/component_updater/hyphenation_component_installer.h"
#include "chrome/browser/component_updater/mei_preload_component_installer.h"
#include "chrome/browser/component_updater/pepper_flash_component_installer.h"
#include "chrome/browser/component_updater/ssl_error_assistant_component_installer.h"
#include "chrome/browser/component_updater/sth_set_component_remover.h"
#include "chrome/browser/component_updater/subresource_filter_component_installer.h"
#include "chrome/browser/component_updater/trust_token_key_commitments_component_installer.h"
#include "chrome/browser/component_updater/zxcvbn_data_component_installer.h"
#include "chrome/common/buildflags.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/pref_names.h"
#include "components/component_updater/component_updater_service.h"
#include "components/component_updater/crl_set_remover.h"
#include "components/component_updater/installer_policies/autofill_states_component_installer.h"
#include "components/component_updater/installer_policies/on_device_head_suggest_component_installer.h"
#include "components/component_updater/installer_policies/optimization_hints_component_installer.h"
#include "components/component_updater/installer_policies/safety_tips_component_installer.h"
#include "components/nacl/common/buildflags.h"
#include "device/vr/buildflags/buildflags.h"
#include "third_party/widevine/cdm/buildflags.h"

#if defined(OS_WIN)
#include "chrome/browser/component_updater/sw_reporter_installer_win.h"
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
#include "chrome/browser/component_updater/third_party_module_list_component_installer_win.h"
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)
#endif  // defined(OS_WIN)

#if defined(OS_WIN)
#include "chrome/browser/component_updater/recovery_improved_component_installer.h"
#else
#include "chrome/browser/component_updater/recovery_component_installer.h"
#endif  // defined(OS_WIN)

#if !defined(OS_ANDROID)
#include "chrome/browser/component_updater/soda_component_installer.h"
#include "chrome/browser/resource_coordinator/tab_manager.h"
#include "media/base/media_switches.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/component_updater/smart_dim_component_installer.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(ENABLE_NACL)
#include "chrome/browser/component_updater/pnacl_component_installer.h"
#endif  // BUILDFLAG(ENABLE_NACL)

#if BUILDFLAG(ENABLE_VR)
#include "chrome/browser/component_updater/vr_assets_component_installer.h"
#endif

#if BUILDFLAG(ENABLE_WIDEVINE_CDM_COMPONENT)
#include "chrome/browser/component_updater/widevine_cdm_component_installer.h"
#endif  // BUILDFLAG(ENABLE_WIDEVINE_CDM_COMPONENT)

#if defined(USE_AURA)
#include "ui/aura/env.h"
#endif

namespace component_updater {

void RegisterComponentsForUpdate(bool is_off_the_record_profile,
                                 PrefService* profile_prefs,
                                 const base::FilePath& profile_path) {
  auto* const cus = g_browser_process->component_updater();

#if defined(OS_WIN)
  RegisterRecoveryImprovedComponent(cus, g_browser_process->local_state());
#else
  // TODO(crbug.com/687231): Implement the Improved component on Mac, etc.
  RegisterRecoveryComponent(cus, g_browser_process->local_state());
#endif  // defined(OS_WIN)

  // TODO(crbug.com/1069814): Remove after 2021-10-01.
  CleanUpPepperFlashComponent(profile_path);

#if BUILDFLAG(ENABLE_WIDEVINE_CDM_COMPONENT)
  RegisterWidevineCdmComponent(cus);
#endif  // BUILDFLAG(ENABLE_WIDEVINE_CDM_COMPONENT)

#if BUILDFLAG(ENABLE_NACL) && !defined(OS_ANDROID)
#if BUILDFLAG(IS_CHROMEOS_ASH)
  // PNaCl on Chrome OS is on rootfs and there is no need to download it. But
  // Chrome4ChromeOS on Linux doesn't contain PNaCl so enable component
  // installer when running on Linux. See crbug.com/422121 for more details.
  if (!base::SysInfo::IsRunningOnChromeOS()) {
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
    RegisterPnaclComponent(cus);
#if BUILDFLAG(IS_CHROMEOS_ASH)
  }
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
#endif  // BUILDFLAG(ENABLE_NACL) && !defined(OS_ANDROID)

  RegisterSubresourceFilterComponent(cus);
  RegisterFlocComponent(cus,
                        g_browser_process->floc_sorting_lsh_clusters_service());
  RegisterOnDeviceHeadSuggestComponent(
      cus, g_browser_process->GetApplicationLocale());
  RegisterOptimizationHintsComponent(cus);
  RegisterTrustTokenKeyCommitmentsComponentIfTrustTokensEnabled(cus);
  RegisterFirstPartySetsComponent(cus);

  base::FilePath path;
  if (base::PathService::Get(chrome::DIR_USER_DATA, &path)) {
    // The CRLSet component previously resided in a different location: delete
    // the old file.
    component_updater::DeleteLegacyCRLSet(path);

#if !BUILDFLAG(IS_CHROMEOS_ASH) && !defined(OS_ANDROID)
    // Clean up previous STH sets that may have been installed. This is not
    // done for:
    // Android: Because STH sets were never used
    // Chrome OS: On Chrome OS, this cleanup is delayed until user login.
    component_updater::DeleteLegacySTHSet(path);
#endif
  }
  RegisterSSLErrorAssistantComponent(cus);
  RegisterFileTypePoliciesComponent(cus);
  RegisterDesktopSharingHubComponent(cus);

#if !BUILDFLAG(IS_CHROMEOS_ASH)
  // CRLSetFetcher attempts to load a CRL set from either the local disk or
  // network.
  // For Chrome OS this registration is delayed until user login.
  component_updater::RegisterCRLSetComponent(cus);
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

  RegisterOriginTrialsComponent(cus);
  RegisterMediaEngagementPreloadComponent(cus, base::OnceClosure());

#if defined(OS_WIN)
  // SwReporter is only needed for official builds.  However, to enable testing
  // on chromium build bots, it is always registered here and
  // RegisterSwReporterComponent() has support for running only in official
  // builds or tests.
  RegisterSwReporterComponent(cus);
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  RegisterThirdPartyModuleListComponent(cus);
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)
#endif  // defined(OS_WIN)

#if BUILDFLAG(ENABLE_VR)
  if (component_updater::ShouldRegisterVrAssetsComponentOnStartup()) {
    component_updater::RegisterVrAssetsComponent(cus);
  }
#endif

  RegisterSafetyTipsComponent(cus);
  RegisterCrowdDenyComponent(cus);

#if BUILDFLAG(IS_CHROMEOS_ASH)
  RegisterSmartDimComponent(cus);
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(USE_MINIKIN_HYPHENATION) && !defined(OS_ANDROID)
  RegisterHyphenationComponent(cus);
#endif

  RegisterZxcvbnDataComponent(cus);

  RegisterAutofillStatesComponent(cus, g_browser_process->local_state());

  RegisterAutofillRegexComponent(cus);
}

}  // namespace component_updater
