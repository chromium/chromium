// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_keeplist_ash.h"

#include <stddef.h>

#include "base/containers/contains.h"
#include "base/no_destructor.h"
#include "base/strings/string_piece.h"
#include "chrome/browser/ash/file_manager/app_id.h"
#include "chrome/browser/ash/input_method/component_extension_ime_manager_delegate_impl.h"
#include "chrome/browser/ui/app_list/arc/arc_app_utils.h"
#include "chrome/common/buildflags.h"
#include "chrome/common/extensions/extension_constants.h"
#include "extensions/common/constants.h"

namespace extensions {

bool ExtensionRunsInAsh(const std::string& extension_id) {
  static base::NoDestructor<std::set<base::StringPiece>> keep_list(
      {extension_misc::kEspeakSpeechSynthesisExtensionId,
       extension_misc::kGoogleSpeechSynthesisExtensionId,
       extension_misc::kEnhancedNetworkTtsExtensionId,
       extension_misc::kSelectToSpeakExtensionId,
       extension_misc::kAccessibilityCommonExtensionId,
       extension_misc::kChromeVoxExtensionId,
       extension_misc::kSwitchAccessExtensionId,
       extension_misc::kSigninProfileTestExtensionId,
       extension_misc::kGuestModeTestExtensionId,
       extension_misc::kKeyboardExtensionId,
       extension_misc::kHelpAppExtensionId, extension_misc::kGCSEExtensionId,
       extension_misc::kGnubbyV3ExtensionId,
       extension_misc::kBruSecurityKeyForwarderExtensionId,
       file_manager::kImageLoaderExtensionId});
  return base::Contains(*keep_list, extension_id) ||
         ash::input_method::ComponentExtensionIMEManagerDelegateImpl::
             IsIMEExtensionID(extension_id);
}

bool ExtensionAppRunsInAsh(const std::string& app_id) {
  static base::NoDestructor<std::set<base::StringPiece>> keep_list(
      {file_manager::kAudioPlayerAppId, extension_misc::kFilesManagerAppId,
       extension_misc::kGoogleKeepAppId, extension_misc::kCalculatorAppId,
       extension_misc::kTextEditorAppId,
       extension_misc::kInAppPaymentsSupportAppId,
       extension_misc::kWallpaperManagerId, arc::kPlayStoreAppId,
       extension_misc::kIdentityApiUiAppId, extension_misc::kGnubbyAppId});
  return base::Contains(*keep_list, app_id);
}

}  // namespace extensions
