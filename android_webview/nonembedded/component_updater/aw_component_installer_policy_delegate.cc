// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/nonembedded/component_updater/aw_component_installer_policy_delegate.h"

#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#include "android_webview/common/aw_paths.h"
#include "android_webview/nonembedded/component_updater/aw_component_update_service.h"
#include "base/android/path_utils.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/logging.h"
#include "base/notreached.h"
#include "base/path_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/values.h"
#include "base/version.h"
#include "components/update_client/utils.h"

namespace android_webview {

namespace {

uint32_t GetHighestSequenceNumber(const base::FilePath& base_file_path) {
  uint32_t highest_sequence_number = 0;
  base::FileEnumerator file_enumerator(base_file_path, /* recursive= */ false,
                                       base::FileEnumerator::DIRECTORIES);
  for (base::FilePath path = file_enumerator.Next(); !path.value().empty();
       path = file_enumerator.Next()) {
    uint32_t sequence_number = 0;
    std::vector<std::string> dir_name_components =
        base::SplitString(path.BaseName().MaybeAsASCII(), "_",
                          base::KEEP_WHITESPACE, base::SPLIT_WANT_ALL);
    if (dir_name_components.size() >= 2 &&
        base::StringToUint(dir_name_components[0], &sequence_number)) {
      highest_sequence_number =
          std::max(sequence_number, highest_sequence_number);
    } else {
      LOG(WARNING) << "FilePath.BaseName isn't on the format of "
                      "<sequence_number>_<version_number>"
                   << path;
    }
  }

  return highest_sequence_number;
}

std::string GetVersionDirName(const uint32_t sequence_number,
                              const std::string& version) {
  return base::NumberToString(sequence_number) + "_" + version;
}

}  // namespace

AwComponentInstallerPolicyDelegate::AwComponentInstallerPolicyDelegate(
    const std::vector<uint8_t>& hash)
    : component_id_(update_client::GetCrxIdFromPublicKeyHash(hash)) {}

AwComponentInstallerPolicyDelegate::~AwComponentInstallerPolicyDelegate() =
    default;

void AwComponentInstallerPolicyDelegate::OnCustomUninstall() {
  // Uninstallation isn't supported in WebView.
  NOTREACHED();
}

// Copy the components file from `install_dir` to the serving directory of the
// java `ComponentsProviderService`. `ComponentsProviderService` serving dir is
// of the form:
//
// <data-dir>/components/cps/<component_id>/<sequence_number>_<version>/...
//
// where `sequence_number` is strictly increasing unsigned integer, with the
// most recent version having the highest `sequence_number`.
//
// Say that there are two versions: .../<sequence_number_1>_<version_1>/ and
// .../<sequence_number_2>_<version_2>/. If `sequence_number_2` >
// `sequence_number_1`, this doesn't necessarily mean that `version_2` >
// `version_1`.
//
// Since this called whenever a valid version is available on disk even if it's
// not recently installed. To avoid doing unneccessry file copying, if the
// highest sequence number maps to the same `version`, this will be a NOOP.
//
// The reason we use a separate sequence number is to make
// `ComponentsProviderService` agnostic to the actual version of the component.
// `ComponentsProviderService` will choose the component with the highest
// sequence number regardless of its version. This will help to tolerate
// downgrading of components versions in the future.
//
// Directories format should be kept in sync with `ComponentsProviderService`
// java class.
void AwComponentInstallerPolicyDelegate::ComponentReady(
    const base::Version& version,
    const base::FilePath& install_dir,
    std::unique_ptr<base::DictionaryValue> manifest) {
  base::FilePath cps_component_base_path =
      GetComponentsProviderServiceDirectory();
  if (cps_component_base_path.empty()) {
    LOG(ERROR) << "Couldn't get component dir";
    return;
  }

  uint32_t highest_sequence_number =
      GetHighestSequenceNumber(cps_component_base_path);

  // Do nothing, if the highest sequence number refers to the same `version`.
  if (base::PathExists(cps_component_base_path.AppendASCII(
          GetVersionDirName(highest_sequence_number, version.GetString())))) {
    DVLOG(1) << cps_component_base_path.AppendASCII(GetVersionDirName(
                    highest_sequence_number, version.GetString()))
             << " already exists.";
    return;
  }

  base::FilePath temp_path_root;
  if (!base::PathService::Get(DIR_COMPONENTS_TEMP, &temp_path_root)) {
    LOG(ERROR) << "Error getting root temp path";
    return;
  }

  base::ScopedTempDir temp_dir;
  if (!temp_dir.CreateUniqueTempDirUnderPath(temp_path_root)) {
    LOG(ERROR) << "Error creating temp file under " << temp_path_root;
    return;
  }

  const std::string new_sequence_version_string =
      GetVersionDirName(highest_sequence_number + 1, version.GetString());
  const base::FilePath temp_copy_path =
      temp_dir.GetPath().AppendASCII(new_sequence_version_string);
  // TODO(crbug.com/1176335) use file links to optimize copies number.
  if (!base::CopyDirectory(install_dir, temp_copy_path,
                           /* recursive= */ true)) {
    LOG(ERROR) << "Error copying from " << install_dir << " to "
               << temp_copy_path;
    return;
  }

  const base::FilePath dest_path =
      cps_component_base_path.AppendASCII(new_sequence_version_string);
  // Always attempt to create the base dir just in case if it doesn't exist.
  base::CreateDirectory(cps_component_base_path);
  if (!base::Move(temp_copy_path, dest_path)) {
    LOG(ERROR) << "Error moving from " << temp_copy_path << " to " << dest_path;
  }
}

base::FilePath
AwComponentInstallerPolicyDelegate::GetComponentsProviderServiceDirectory() {
  base::FilePath path;
  if (!base::android::GetDataDirectory(&path)) {
    LOG(ERROR) << "Couldn't get Android data directory";
    return path;
  }
  return path.AppendASCII("components")
      .AppendASCII("cps")
      .AppendASCII(component_id_);
}

}  // namespace android_webview
