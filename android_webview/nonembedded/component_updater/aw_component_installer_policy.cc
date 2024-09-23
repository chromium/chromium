// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/nonembedded/component_updater/aw_component_installer_policy.h"

#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#include "android_webview/common/aw_paths.h"
#include "android_webview/nonembedded/component_updater/aw_component_update_service.h"
#include "base/android/jni_string.h"
#include "base/android/path_utils.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/logging.h"
#include "base/notreached.h"
#include "base/path_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/values.h"
#include "base/version.h"
#include "components/update_client/utils.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "android_webview/nonembedded/nonembedded_jni_headers/ComponentsProviderPathUtil_jni.h"

namespace android_webview {

namespace {

std::string GetVersionDirName(const uint32_t sequence_number,
                              const std::string& version) {
  return base::NumberToString(sequence_number) + "_" + version;
}

}  // namespace

AwComponentInstallerPolicy::AwComponentInstallerPolicy() = default;

void AwComponentInstallerPolicy::OnCustomUninstall() {
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
void AwComponentInstallerPolicy::ComponentReady(
    const base::Version& version,
    const base::FilePath& install_dir,
    base::Value::Dict manifest) {
  base::FilePath cps_component_base_path =
      GetComponentsProviderServiceDirectory();

  JNIEnv* env = jni_zero::AttachCurrentThread();
  int highest_sequence_number =
      Java_ComponentsProviderPathUtil_getTheHighestSequenceNumber(
          env, base::android::ConvertUTF8ToJavaString(
                   env, cps_component_base_path.MaybeAsASCII()));

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
  // TODO(crbug.com/40747851) use file links to optimize copies number.
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
    return;
  }

  IncrementComponentsUpdatedCount();
}

void AwComponentInstallerPolicy::IncrementComponentsUpdatedCount() {
  AwComponentUpdateService::GetInstance()->IncrementComponentsUpdatedCount();
}

base::FilePath
AwComponentInstallerPolicy::GetComponentsProviderServiceDirectory() {
  std::vector<uint8_t> hash;
  GetHash(&hash);
  std::string component_id = update_client::GetCrxIdFromPublicKeyHash(hash);

  JNIEnv* env = jni_zero::AttachCurrentThread();
  return base::FilePath(
             base::android::ConvertJavaStringToUTF8(
                 env,
                 Java_ComponentsProviderPathUtil_getComponentsServingDirectoryPath(
                     env)))
      .AppendASCII(component_id);
}

}  // namespace android_webview
