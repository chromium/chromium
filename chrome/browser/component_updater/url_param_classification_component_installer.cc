// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/component_updater/url_param_classification_component_installer.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/metrics/histogram_functions.h"
#include "base/path_service.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/version.h"
#include "chrome/browser/url_param_filter/url_param_classifications_loader.h"
#include "chrome/common/chrome_features.h"
#include "components/component_updater/component_updater_paths.h"
#include "content/public/browser/browser_task_traits.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

using component_updater::ComponentUpdateService;

namespace {

const base::FilePath::CharType kUrlParamClassificationsFileName[] =
    FILE_PATH_LITERAL("list.pb");

// The SHA256 of the SubjectPublicKeyInfo used to sign the extension.
// The extension id is: dnhnnofocefcglhjeigmkhcgfoaipbaa
const uint8_t kUrlParamClassificationsPublicKeySHA256[32] = {
    0x3d, 0x7d, 0xde, 0x5e, 0x24, 0x52, 0x6b, 0x79, 0x48, 0x6c, 0xa7,
    0x26, 0x5e, 0x08, 0xf1, 0x00, 0x82, 0x56, 0x69, 0xb1, 0xca, 0xfc,
    0x8a, 0x25, 0x50, 0x06, 0x6b, 0x5f, 0xa1, 0xd5, 0xeb, 0x80};

const char kUrlParamClassificationManifestName[] = "Url Param Classifications";

// Runs on a thread pool.
absl::optional<std::string> LoadFileFromDisk(const base::FilePath& pb_path) {
  VLOG(1) << "Reading Url Param Classifications from file: " << pb_path.value();
  std::string file_contents;
  if (!base::ReadFileToString(pb_path, &file_contents)) {
    // The file won't exist on new installations, so this is not always an
    // error.
    VLOG(1) << "Failed reading from " << pb_path.value();
    return absl::nullopt;
  }
  return file_contents;
}

// Writes a metric denoting the |result| of validating a classification list.
//
// This method is called in VerifyInstallation which returns false (on an error)
// or true (if the whole list is valid), so the metrics will be populated at
// most once per version installed.
void WriteMetrics(
    component_updater::UrlParamClassificationComponentInstallerPolicy::
        ClassificationListValidationResult result) {
  base::UmaHistogramEnumeration(
      "Navigation.UrlParamFilter.ClassificationListValidationResult", result);
}

}  // namespace

namespace component_updater {

UrlParamClassificationComponentInstallerPolicy::
    UrlParamClassificationComponentInstallerPolicy(
        component_updater::OnUrlParamClassificationComponentReady
            on_component_ready)
    : on_component_ready_(on_component_ready) {}

UrlParamClassificationComponentInstallerPolicy::
    ~UrlParamClassificationComponentInstallerPolicy() = default;

bool UrlParamClassificationComponentInstallerPolicy::
    SupportsGroupPolicyEnabledComponentUpdates() const {
  return true;
}

bool UrlParamClassificationComponentInstallerPolicy::RequiresNetworkEncryption()
    const {
  return false;
}

update_client::CrxInstaller::Result
UrlParamClassificationComponentInstallerPolicy::OnCustomInstall(
    const base::Value& manifest,
    const base::FilePath& install_dir) {
  return update_client::CrxInstaller::Result(0);  // Nothing custom here.
}

void UrlParamClassificationComponentInstallerPolicy::OnCustomUninstall() {}

base::FilePath UrlParamClassificationComponentInstallerPolicy::GetInstalledPath(
    const base::FilePath& base) {
  return base.Append(kUrlParamClassificationsFileName);
}

void UrlParamClassificationComponentInstallerPolicy::ComponentReady(
    const base::Version& version,
    const base::FilePath& install_dir,
    base::Value manifest) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  VLOG(1) << "Component ready, version " << version.GetString() << " in "
          << install_dir.value();
  if (base::FeatureList::IsEnabled(features::kIncognitoParamFilterEnabled)) {
    // Given BEST_EFFORT since we don't need to be USER_BLOCKING.
    base::ThreadPool::PostTaskAndReplyWithResult(
        FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
        base::BindOnce(&LoadFileFromDisk, GetInstalledPath(install_dir)),
        base::BindOnce(
            [](OnUrlParamClassificationComponentReady callback,
               const absl::optional<std::string>& maybe_file) {
              if (maybe_file.has_value())
                callback.Run(maybe_file.value());
            },
            on_component_ready_));
  }
}

// Called during startup and installation before ComponentReady().
bool UrlParamClassificationComponentInstallerPolicy::VerifyInstallation(
    const base::Value& manifest,
    const base::FilePath& install_dir) const {
  if (!base::PathExists(GetInstalledPath(install_dir))) {
    WriteMetrics(
        ClassificationListValidationResult::kMissingClassificationsFile);
    return false;
  }

  std::string file_contents;
  if (!base::ReadFileToString(GetInstalledPath(install_dir), &file_contents)) {
    WriteMetrics(
        ClassificationListValidationResult::kReadingClassificationsFileFailed);
    return false;
  }

  url_param_filter::FilterClassifications classification_list;
  if (!classification_list.ParseFromString(file_contents)) {
    WriteMetrics(ClassificationListValidationResult::kParsingToProtoFailed);
    return false;
  }

  std::vector<url_param_filter::FilterClassification> source_classifications,
      destination_classifications;
  for (const url_param_filter::FilterClassification& fc :
       classification_list.classifications()) {
    if (!fc.has_site()) {
      WriteMetrics(
          ClassificationListValidationResult::kClassificationMissingSite);
      return false;
    }

    if (!fc.has_site_role()) {
      WriteMetrics(
          ClassificationListValidationResult::kClassificationMissingSiteRole);
      return false;
    }

    if (fc.site_role() ==
        url_param_filter::FilterClassification_SiteRole_SOURCE) {
      source_classifications.push_back(fc);
    }

    if (fc.site_role() ==
        url_param_filter::FilterClassification_SiteRole_DESTINATION) {
      destination_classifications.push_back(fc);
    }
  }

  WriteMetrics(ClassificationListValidationResult::kSuccessful);
  return true;
}

base::FilePath
UrlParamClassificationComponentInstallerPolicy::GetRelativeInstallDir() const {
  return base::FilePath(FILE_PATH_LITERAL("UrlParamClassifications"));
}

void UrlParamClassificationComponentInstallerPolicy::GetHash(
    std::vector<uint8_t>* hash) const {
  hash->assign(std::begin(kUrlParamClassificationsPublicKeySHA256),
               std::end(kUrlParamClassificationsPublicKeySHA256));
}

std::string UrlParamClassificationComponentInstallerPolicy::GetName() const {
  return kUrlParamClassificationManifestName;
}

update_client::InstallerAttributes
UrlParamClassificationComponentInstallerPolicy::GetInstallerAttributes() const {
  return update_client::InstallerAttributes();
}

void RegisterUrlParamClassificationComponent(ComponentUpdateService* cus) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  // Register the component even if feature isn't enabled so that when it is
  // enabled in the future, the component is already installed.
  VLOG(1) << "Registering Url Param Classifications component.";
  auto installer = base::MakeRefCounted<ComponentInstaller>(
      std::make_unique<UrlParamClassificationComponentInstallerPolicy>(
          base::BindRepeating([](std::string raw_classifications) {
            url_param_filter::ClassificationsLoader::GetInstance()
                ->ReadClassifications(raw_classifications);
          })));
  installer->Register(cus, base::OnceClosure());
}

}  // namespace component_updater
