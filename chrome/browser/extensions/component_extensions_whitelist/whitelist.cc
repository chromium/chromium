// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/component_extensions_whitelist/whitelist.h"

#include <stddef.h>

#include "base/logging.h"
#include "base/stl_util.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "chrome/common/buildflags.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/grit/browser_resources.h"
#include "extensions/common/constants.h"
#include "printing/buildflags/buildflags.h"

#if defined(OS_CHROMEOS)
#include "ash/keyboard/ui/grit/keyboard_resources.h"
#include "chrome/browser/chromeos/input_method/component_extension_ime_manager_impl.h"
#include "ui/file_manager/grit/file_manager_resources.h"
#endif

namespace extensions {

bool IsComponentExtensionWhitelisted(const std::string& extension_id) {
  const char* const kAllowed[] = {
    extension_misc::kInAppPaymentsSupportAppId,
    extension_misc::kMediaRouterStableExtensionId,
    extension_misc::kPdfExtensionId,
#if defined(OS_CHROMEOS)
    extension_misc::kAssessmentAssistantExtensionId,
    extension_misc::kAutoclickExtensionId,
    extension_misc::kChromeVoxExtensionId,
    extension_misc::kEspeakSpeechSynthesisExtensionId,
    extension_misc::kGoogleSpeechSynthesisExtensionId,
    extension_misc::kSelectToSpeakExtensionId,
    extension_misc::kSwitchAccessExtensionId,
    extension_misc::kZipArchiverExtensionId,
    extension_misc::kChromeCameraAppId,
#endif
  };

  for (size_t i = 0; i < base::size(kAllowed); ++i) {
    if (extension_id == kAllowed[i])
      return true;
  }

#if defined(OS_CHROMEOS)
  if (chromeos::ComponentExtensionIMEManagerImpl::IsIMEExtensionID(
          extension_id)) {
    return true;
  }
#endif
  LOG(ERROR) << "Component extension with id " << extension_id << " not in "
             << "whitelist and is not being loaded as a result.";
  NOTREACHED();
  return false;
}

bool IsComponentExtensionWhitelisted(int manifest_resource_id) {
  switch (manifest_resource_id) {
    // Please keep the list in alphabetical order.
#if BUILDFLAG(ENABLE_PRINTING)
    case IDR_CLOUDPRINT_MANIFEST:
#endif
    case IDR_CRYPTOTOKEN_MANIFEST:
    case IDR_FEEDBACK_MANIFEST:
#if BUILDFLAG(ENABLE_HANGOUT_SERVICES_EXTENSION)
    case IDR_HANGOUT_SERVICES_MANIFEST:
#endif
    case IDR_IDENTITY_API_SCOPE_APPROVAL_MANIFEST:
    case IDR_NETWORK_SPEECH_SYNTHESIS_MANIFEST:
    case IDR_WEBSTORE_MANIFEST:

#if defined(OS_CHROMEOS)
    // Separate ChromeOS list, as it is quite large.
    case IDR_ARC_SUPPORT_MANIFEST:
    case IDR_AUDIO_PLAYER_MANIFEST:
    case IDR_CHROME_APP_MANIFEST:
    case IDR_CONNECTIVITY_DIAGNOSTICS_LAUNCHER_MANIFEST:
    case IDR_CONNECTIVITY_DIAGNOSTICS_MANIFEST:
    case IDR_CROSH_BUILTIN_MANIFEST:
    case IDR_DEMO_APP_MANIFEST:
    case IDR_ECHO_MANIFEST:
    case IDR_FILEMANAGER_MANIFEST:
    case IDR_FIRST_RUN_DIALOG_MANIFEST:
    case IDR_GALLERY_MANIFEST:
    case IDR_IMAGE_LOADER_MANIFEST:
    case IDR_KEYBOARD_MANIFEST:
    case IDR_MOBILE_MANIFEST:
    case IDR_VIDEO_PLAYER_MANIFEST:
    case IDR_WALLPAPERMANAGER_MANIFEST:
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
    case IDR_GENIUS_APP_MANIFEST:
    case IDR_HELP_MANIFEST:
    case IDR_QUICKOFFICE_MANIFEST:
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)
#endif  // defined(OS_CHROMEOS)
      return true;
  }

  LOG(ERROR) << "Component extension with manifest resource id "
             << manifest_resource_id << " not in whitelist and is not being "
             << "loaded as a result.";
  NOTREACHED();
  return false;
}

#if defined(OS_CHROMEOS)
bool IsComponentExtensionWhitelistedForSignInProfile(
    const std::string& extension_id) {
  const char* const kAllowed[] = {
      extension_misc::kAutoclickExtensionId,
      extension_misc::kChromeVoxExtensionId,
      extension_misc::kEspeakSpeechSynthesisExtensionId,
      extension_misc::kGoogleSpeechSynthesisExtensionId,
      extension_misc::kSelectToSpeakExtensionId,
      extension_misc::kSwitchAccessExtensionId,
  };

  for (size_t i = 0; i < base::size(kAllowed); ++i) {
    if (extension_id == kAllowed[i])
      return true;
  }

  return false;
}
#endif

}  // namespace extensions
