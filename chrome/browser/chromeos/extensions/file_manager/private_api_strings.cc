// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/file_manager/private_api_strings.h"

#include <memory>
#include <utility>

#include "ash/public/cpp/ash_features.h"
#include "base/feature_list.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/chromeos/crostini/crostini_features.h"
#include "chrome/browser/chromeos/file_manager/file_manager_string_util.h"
#include "chrome/browser/chromeos/login/demo_mode/demo_session.h"
#include "chrome/browser/chromeos/plugin_vm/plugin_vm_features.h"
#include "chrome/browser/chromeos/plugin_vm/plugin_vm_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_features.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/constants/chromeos_features.h"
#include "chromeos/system/statistics_provider.h"
#include "components/arc/arc_features.h"
#include "extensions/common/extension_l10n_util.h"
#include "ui/base/l10n/l10n_util.h"

namespace extensions {

FileManagerPrivateGetStringsFunction::FileManagerPrivateGetStringsFunction() =
    default;

FileManagerPrivateGetStringsFunction::~FileManagerPrivateGetStringsFunction() =
    default;

ExtensionFunction::ResponseAction FileManagerPrivateGetStringsFunction::Run() {
  auto dict = GetFileManagerStrings();

  dict->SetBoolean("HIDE_SPACE_INFO",
                   chromeos::DemoSession::IsDeviceInDemoMode());
  dict->SetBoolean("ARC_USB_STORAGE_UI_ENABLED",
                   base::FeatureList::IsEnabled(arc::kUsbStorageUIFeature));
  dict->SetBoolean("CROSTINI_ENABLED",
                   crostini::CrostiniFeatures::Get()->IsEnabled(
                       Profile::FromBrowserContext(browser_context())));
  dict->SetBoolean("PLUGIN_VM_ENABLED",
                   plugin_vm::PluginVmFeatures::Get()->IsEnabled(
                       Profile::FromBrowserContext(browser_context())));
  dict->SetBoolean(
      "FILES_CAMERA_FOLDER_ENABLED",
      base::FeatureList::IsEnabled(chromeos::features::kFilesCameraFolder));
  dict->SetBoolean("FILES_NG_ENABLED",
                   base::FeatureList::IsEnabled(chromeos::features::kFilesNG));
  dict->SetBoolean("COPY_IMAGE_ENABLED",
                   base::FeatureList::IsEnabled(
                       chromeos::features::kEnableFilesAppCopyImage));
  dict->SetBoolean(
      "UNIFIED_MEDIA_VIEW_ENABLED",
      base::FeatureList::IsEnabled(chromeos::features::kUnifiedMediaView));
  dict->SetBoolean(
      "FILES_TRANSFER_DETAILS_ENABLED",
      base::FeatureList::IsEnabled(chromeos::features::kFilesTransferDetails));
  dict->SetBoolean("ZIP_MOUNT", base::FeatureList::IsEnabled(
                                    chromeos::features::kFilesZipMount));
  dict->SetBoolean("ZIP_PACK", base::FeatureList::IsEnabled(
                                   chromeos::features::kFilesZipPack));
  dict->SetBoolean("ZIP_UNPACK", base::FeatureList::IsEnabled(
                                     chromeos::features::kFilesZipUnpack));
  dict->SetBoolean("SHARESHEET_ENABLED",
                   base::FeatureList::IsEnabled(features::kSharesheet));
  dict->SetBoolean(
      "FILTERS_IN_RECENTS_ENABLED",
      base::FeatureList::IsEnabled(chromeos::features::kFiltersInRecents));
  dict->SetBoolean("HOLDING_SPACE_ENABLED",
                   ash::features::IsTemporaryHoldingSpaceEnabled());
  dict->SetBoolean("FILES_SINGLE_PARTITION_FORMAT_ENABLED",
                   base::FeatureList::IsEnabled(
                       chromeos::features::kFilesSinglePartitionFormat));

  dict->SetString("UI_LOCALE", extension_l10n_util::CurrentLocaleOrDefault());

  return RespondNow(OneArgument(std::move(dict)));
}

}  // namespace extensions
