// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/component_extensions_allowlist/allowlist.h"

#include <stddef.h>

#include "base/containers/fixed_flat_set.h"
#include "base/logging.h"
#include "base/notreached.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/common/buildflags.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/grit/browser_resources.h"
#include "extensions/common/constants.h"
#include "printing/buildflags/buildflags.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chromeos/constants/chromeos_features.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/keyboard/ui/grit/keyboard_resources.h"
#include "chrome/browser/ash/input_method/component_extension_ime_manager_delegate_impl.h"
#include "ui/file_manager/grit/file_manager_resources.h"
#endif

namespace extensions {

bool IsComponentExtensionAllowlisted(const std::string& extension_id) {
  constexpr auto kAllowed = base::MakeFixedFlatSet<std::string_view>({
      extension_misc::kInAppPaymentsSupportAppId,
      extension_misc::kPdfExtensionId,
#if BUILDFLAG(IS_CHROMEOS)
      extension_misc::kAssessmentAssistantExtensionId,
#endif
#if BUILDFLAG(IS_CHROMEOS_ASH)
      extension_misc::kAccessibilityCommonExtensionId,
      extension_misc::kChromeVoxExtensionId,
      extension_misc::kEnhancedNetworkTtsExtensionId,
      extension_misc::kEspeakSpeechSynthesisExtensionId,
      extension_misc::kGoogleSpeechSynthesisExtensionId,
      extension_misc::kGuestModeTestExtensionId,
      extension_misc::kSelectToSpeakExtensionId,
      extension_misc::kSwitchAccessExtensionId,
#elif BUILDFLAG(IS_CHROMEOS_LACROS)
      extension_misc::kEmbeddedA11yHelperExtensionId,
      extension_misc::kChromeVoxHelperExtensionId,
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)
#if BUILDFLAG(IS_CHROMEOS)
      extension_misc::kContactCenterInsightsExtensionId,
      extension_misc::kDeskApiExtensionId,
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
      extension_misc::kQuickOfficeComponentExtensionId,
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)
#endif
#if !BUILDFLAG(IS_CHROMEOS_LACROS)
      extension_misc::kReadingModeGDocsHelperExtensionId,
#endif  // !BUILDFLAG(IS_CHROMEOS_LACROS)
  });

  if (kAllowed.contains(extension_id)) {
    return true;
  }

#if BUILDFLAG(IS_CHROMEOS)
  if (chromeos::features::IsUploadOfficeToCloudEnabled() &&
      extension_id == extension_misc::kODFSExtensionId) {
    return true;
  }
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (ash::input_method::ComponentExtensionIMEManagerDelegateImpl::
          IsIMEExtensionID(extension_id)) {
    return true;
  }
#endif
  LOG(ERROR) << "Component extension with id " << extension_id << " not in "
             << "allowlist and is not being loaded as a result.";
  NOTREACHED_IN_MIGRATION();
  return false;
}

bool IsComponentExtensionAllowlisted(int manifest_resource_id) {
  switch (manifest_resource_id) {
    // Please keep the list in alphabetical order.
#if BUILDFLAG(ENABLE_HANGOUT_SERVICES_EXTENSION)
    case IDR_HANGOUT_SERVICES_MANIFEST_V2:
    case IDR_HANGOUT_SERVICES_MANIFEST_V3:
#endif
    case IDR_NETWORK_SPEECH_SYNTHESIS_MANIFEST:
    case IDR_READING_MODE_GDOCS_HELPER_MANIFEST:
    case IDR_WEBSTORE_MANIFEST:

#if BUILDFLAG(IS_CHROMEOS_ASH)
    // Separate ChromeOS list, as it is quite large.
    case IDR_ARC_SUPPORT_MANIFEST:
    case IDR_CHROME_APP_MANIFEST:
    case IDR_IMAGE_LOADER_MANIFEST:
    case IDR_KEYBOARD_MANIFEST:
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
    case IDR_HELP_MANIFEST:
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_CHROMEOS)
    case IDR_CONTACT_CENTER_INSIGHTS_MANIFEST:
    case IDR_DESK_API_MANIFEST:
    case IDR_ECHO_MANIFEST:
#endif  // BUILDFLAG(IS_CHROMEOS)
      return true;
  }

  LOG(ERROR) << "Component extension with manifest resource id "
             << manifest_resource_id << " not in allowlist and is not being "
             << "loaded as a result.";
  NOTREACHED_IN_MIGRATION();
  return false;
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
bool IsComponentExtensionAllowlistedForSignInProfile(
    const std::string& extension_id) {
  constexpr auto kAllowed = base::MakeFixedFlatSet<std::string_view>({
      extension_misc::kAccessibilityCommonExtensionId,
      extension_misc::kChromeVoxExtensionId,
      extension_misc::kEspeakSpeechSynthesisExtensionId,
      extension_misc::kGoogleSpeechSynthesisExtensionId,
      extension_misc::kSelectToSpeakExtensionId,
      extension_misc::kSwitchAccessExtensionId,
  });

  return kAllowed.contains(extension_id);
}
#endif

}  // namespace extensions
