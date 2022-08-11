// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_keeplist_chromeos.h"

#include <stddef.h>
#include <set>

#include "base/containers/contains.h"
#include "base/no_destructor.h"
#include "base/strings/string_piece.h"
#include "build/chromeos_buildflags.h"
#include "chrome/common/buildflags.h"
#include "chrome/common/extensions/extension_constants.h"
#include "extensions/common/constants.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/file_manager/app_id.h"
#include "chrome/browser/ash/input_method/component_extension_ime_manager_delegate_impl.h"
#include "chrome/browser/ui/app_list/arc/arc_app_utils.h"
#endif

namespace extensions {

bool ExtensionRunsInBothOSAndStandaloneBrowser(
    const std::string& extension_id) {
  static base::NoDestructor<std::set<base::StringPiece>> keep_list({
      extension_misc::kGCSEExtensionId,
      extension_misc::kGnubbyV3ExtensionId,
  });
  return base::Contains(*keep_list, extension_id);
}

bool ExtensionAppRunsInBothOSAndStandaloneBrowser(
    const std::string& extension_id) {
  static base::NoDestructor<std::set<base::StringPiece>> keep_list({
      extension_misc::kGnubbyAppId,
  });
  return base::Contains(*keep_list, extension_id);
}

bool ExtensionRunsInOS(const std::string& extension_id) {
  static base::NoDestructor<std::set<base::StringPiece>> keep_list({
#if BUILDFLAG(IS_CHROMEOS_ASH)
    extension_misc::kEspeakSpeechSynthesisExtensionId,
        extension_misc::kGoogleSpeechSynthesisExtensionId,
        extension_misc::kEnhancedNetworkTtsExtensionId,
        extension_misc::kSelectToSpeakExtensionId,
        extension_misc::kAccessibilityCommonExtensionId,
        extension_misc::kSwitchAccessExtensionId,
        extension_misc::kSigninProfileTestExtensionId,
        extension_misc::kGuestModeTestExtensionId,
        extension_misc::kHelpAppExtensionId,
        file_manager::kImageLoaderExtensionId,
#endif
        extension_misc::kKeyboardExtensionId,
        extension_misc::kChromeVoxExtensionId,
        extension_misc::kBruSecurityKeyForwarderExtensionId,
  });
  bool is_ime = false;
#if BUILDFLAG(IS_CHROMEOS_ASH)
  is_ime = ash::input_method::ComponentExtensionIMEManagerDelegateImpl::
      IsIMEExtensionID(extension_id);
#endif

  return ExtensionRunsInBothOSAndStandaloneBrowser(extension_id) ||
         base::Contains(*keep_list, extension_id) || is_ime;
}

bool ExtensionAppRunsInOS(const std::string& app_id) {
  static base::NoDestructor<std::set<base::StringPiece>> keep_list({
#if BUILDFLAG(IS_CHROMEOS_ASH)
    arc::kPlayStoreAppId, extension_misc::kFilesManagerAppId,
#endif

        extension_misc::kGoogleKeepAppId, extension_misc::kCalculatorAppId,
        extension_misc::kInAppPaymentsSupportAppId,
        extension_misc::kIdentityApiUiAppId
  });
  return ExtensionAppRunsInBothOSAndStandaloneBrowser(app_id) ||
         base::Contains(*keep_list, app_id);
}

}  // namespace extensions
