// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/component_updater/registration.h"

#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/metrics/histogram_functions.h"
#include "base/path_service.h"
#include "base/task/thread_pool.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/buildflags.h"
#include "chrome/browser/component_updater/app_provisioning_component_installer.h"
#include "chrome/browser/component_updater/captcha_provider_component_installer.h"
#include "chrome/browser/component_updater/chrome_origin_trials_component_installer.h"
#include "chrome/browser/component_updater/commerce_heuristics_component_installer.h"
#include "chrome/browser/component_updater/crl_set_component_installer.h"
#include "chrome/browser/component_updater/crowd_deny_component_installer.h"
#include "chrome/browser/component_updater/first_party_sets_component_installer.h"
#include "chrome/browser/component_updater/hyphenation_component_installer.h"
#include "chrome/browser/component_updater/mei_preload_component_installer.h"
#include "chrome/browser/component_updater/pki_metadata_component_installer.h"
#include "chrome/browser/component_updater/privacy_sandbox_attestations_component_installer.h"
#include "chrome/browser/component_updater/ssl_error_assistant_component_installer.h"
#include "chrome/browser/component_updater/subresource_filter_component_installer.h"
#include "chrome/browser/component_updater/trust_token_key_commitments_component_installer.h"
#include "chrome/browser/history_embeddings/history_embeddings_utils.h"
#include "chrome/common/buildflags.h"
#include "chrome/common/chrome_paths.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "components/component_updater/component_updater_service.h"
#include "components/component_updater/installer_policies/history_search_strings_component_installer.h"
#include "components/component_updater/installer_policies/on_device_head_suggest_component_installer.h"
#include "components/component_updater/installer_policies/optimization_hints_component_installer.h"
#include "components/component_updater/installer_policies/plus_address_blocklist_component_installer.h"
#include "components/component_updater/installer_policies/safety_tips_component_installer.h"
#include "components/on_device_translation/buildflags/buildflags.h"
#include "components/safe_browsing/core/common/features.h"
#include "device/vr/buildflags/buildflags.h"
#include "third_party/widevine/cdm/buildflags.h"
#include "ui/accessibility/accessibility_features.h"

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
#include "chrome/browser/component_updater/recovery_improved_component_installer.h"
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)

#if BUILDFLAG(IS_ANDROID)
#include "chrome/browser/component_updater/real_time_url_checks_allowlist_component_installer.h"
#else
#include "chrome/browser/component_updater/screen_ai_component_installer.h"
#endif  // BUILDFLAG(IS_ANDROID)

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/component_updater/actor_safety_lists_component_installer.h"
#include "chrome/browser/component_updater/iwa_key_distribution_component_installer.h"
#include "chrome/browser/component_updater/zxcvbn_data_component_installer.h"
#include "chrome/browser/resource_coordinator/tab_manager.h"
#include "media/base/media_switches.h"
#endif  // !BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_CHROMEOS)
#include "base/system/sys_info.h"
#include "chrome/browser/apps/app_service/chrome_app_deprecation/chrome_app_deprecation.h"
#include "chrome/browser/component_updater/smart_dim_component_installer.h"
#endif  // BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(ENABLE_MEDIA_FOUNDATION_WIDEVINE_CDM)
#include "chrome/browser/component_updater/media_foundation_widevine_cdm_component_installer.h"
#endif

#if BUILDFLAG(ENABLE_ON_DEVICE_TRANSLATION)
#include "chrome/browser/component_updater/translate_kit_component_installer.h"
#include "chrome/browser/component_updater/translate_kit_language_pack_component_installer.h"
#endif  // BUILDFLAG(ENABLE_ON_DEVICE_TRANSLATION)

#if BUILDFLAG(ENABLE_WIDEVINE_CDM_COMPONENT)
#include "chrome/browser/component_updater/widevine_cdm_component_installer.h"
#endif  // BUILDFLAG(ENABLE_WIDEVINE_CDM_COMPONENT)

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_ANDROID)
#include "components/component_updater/installer_policies/amount_extraction_heuristic_regexes_component_installer.h"
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) ||
        // BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_ANDROID)

#if defined(USE_AURA)
#include "ui/aura/env.h"
#endif

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
#include "chrome/browser/component_updater/wasm_tts_engine_component_installer.h"
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)

#if BUILDFLAG(SAFE_BROWSING_AVAILABLE)
#include "chrome/browser/component_updater/file_type_policies_component_installer.h"
#endif

namespace component_updater {

namespace {

// Runs in the thread pool, may block.
void DeleteOldComponents(const base::FilePath& user_data_dir) {
  for (const base::FilePath::StringType& dir : {
           FILE_PATH_LITERAL("MaskedDomainListPreloaded"),  // Remove in M146+
           FILE_PATH_LITERAL("DesktopSharingHub"),          // Remove in M146+
           FILE_PATH_LITERAL("CookieReadinessList"),        // Remove in M146+
           FILE_PATH_LITERAL("OpenCookieDatabase"),         // Remove in M146+
           FILE_PATH_LITERAL("TpcdMetadata"),               // Remove in M147+
           FILE_PATH_LITERAL(
               "ProbabilisticRevealTokenRegistry"),  // Remove in M148+
           FILE_PATH_LITERAL("AutofillStates"),      // Remove in M153+
           FILE_PATH_LITERAL(
               "Fingerprinting Protection Filter"),  // Remove in M156+
#if BUILDFLAG(IS_CHROMEOS)
           // TODO(crbug.com/380780352): Remove these after the stepping stone.
           FILE_PATH_LITERAL("lacros-dogfood-canary"),
           FILE_PATH_LITERAL("lacros-dogfood-dev"),
           FILE_PATH_LITERAL("lacros-dogfood-beta"),
           FILE_PATH_LITERAL("lacros-dogfood-stable"),
#endif  // BUILDFLAG(IS_CHROMEOS)
       }) {
    base::DeletePathRecursively(user_data_dir.Append(dir));
  }
}

}  // namespace

void RegisterComponentsForUpdate() {
  auto* const cus = g_browser_process->component_updater();

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
  RegisterRecoveryImprovedComponent(cus, g_browser_process->local_state());
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)

#if BUILDFLAG(ENABLE_MEDIA_FOUNDATION_WIDEVINE_CDM)
  RegisterMediaFoundationWidevineCdmComponent(cus);
#endif

#if BUILDFLAG(ENABLE_WIDEVINE_CDM_COMPONENT)
  RegisterWidevineCdmComponent(cus);
#endif  // BUILDFLAG(ENABLE_WIDEVINE_CDM_COMPONENT)

  RegisterSubresourceFilterComponent(cus);
  RegisterOnDeviceHeadSuggestComponent(
      cus, g_browser_process->GetApplicationLocale());
  RegisterOptimizationHintsComponent(cus);
  RegisterTrustTokenKeyCommitmentsComponentIfTrustTokensEnabled(cus);
  RegisterFirstPartySetsComponent(cus);
  RegisterPrivacySandboxAttestationsComponent(cus);
  if (history_embeddings::IsHistoryEmbeddingsFeatureEnabled()) {
    RegisterHistorySearchStringsComponent(cus);
  }
  RegisterSSLErrorAssistantComponent(cus);

#if BUILDFLAG(SAFE_BROWSING_AVAILABLE)
  RegisterFileTypePoliciesComponent(cus);
#endif

#if !BUILDFLAG(IS_CHROMEOS)
  // CRLSetFetcher attempts to load a CRL set from either the local disk or
  // network.
  // For Chrome OS this registration is delayed until user login.
  component_updater::RegisterCRLSetComponent(cus);
#endif  // !BUILDFLAG(IS_CHROMEOS)

  RegisterOriginTrialsComponent(cus);
  RegisterMediaEngagementPreloadComponent(cus, base::OnceClosure());

  MaybeRegisterPKIMetadataComponent(cus);

  RegisterSafetyTipsComponent(cus);
  RegisterCrowdDenyComponent(cus);

#if BUILDFLAG(IS_CHROMEOS)
  RegisterSmartDimComponent(cus);
  RegisterAppProvisioningComponent(cus);
  apps::chrome_app_deprecation::RegisterAllowlistComponentUpdater(cus);
#endif  // BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(USE_MINIKIN_HYPHENATION) && !BUILDFLAG(IS_ANDROID)
  RegisterHyphenationComponent(cus);
#endif

#if !BUILDFLAG(IS_ANDROID)
  RegisterIwaKeyDistributionComponent(cus);
  RegisterZxcvbnDataComponent(cus);
  RegisterActorSafetyListsComponent(cus, base::OnceClosure());
#endif  // !BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_ANDROID)
  RegisterRealTimeUrlChecksAllowlistComponent(cus);
#endif  // BUIDLFLAG(IS_ANDROID)

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_CHROMEOS)
  ManageScreenAIComponentRegistration(cus, g_browser_process->local_state());
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_CHROMEOS)

  RegisterCommerceHeuristicsComponent(cus);

  RegisterPlusAddressBlocklistComponent(cus);

#if BUILDFLAG(ENABLE_ON_DEVICE_TRANSLATION)
  // TODO(crbug.com/364795294): Support other platforms.
  RegisterTranslateKitComponent(cus, g_browser_process->local_state(),
                                /*force_install=*/false,
                                /*registered_callback=*/base::OnceClosure(),
                                /*on_ready_callback=*/base::DoNothing());
  RegisterTranslateKitLanguagePackComponentsForUpdate(
      cus, g_browser_process->local_state());
  RegisterTranslateKitLanguagePackComponentsForAutoDownload(
      cus, g_browser_process->local_state());
#endif  // BUILDFLAG(ENABLE_ON_DEVICE_TRANSLATION)

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_ANDROID)
  RegisterAmountExtractionHeuristicRegexesComponent(cus);
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) ||
        // BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
  RegisterWasmTtsEngineComponent(cus, g_browser_process->local_state());
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)

  RegisterCaptchaProviderComponent(cus);

  base::FilePath path;
  if (base::PathService::Get(chrome::DIR_USER_DATA, &path)) {
    if (!history_embeddings::IsHistoryEmbeddingsFeatureEnabled()) {
      DeleteHistorySearchStringsComponent(path);
    }
    base::ThreadPool::PostTask(
        FROM_HERE, {base::TaskPriority::BEST_EFFORT, base::MayBlock()},
        base::BindOnce(&DeleteOldComponents, path));
  }
}

}  // namespace component_updater
