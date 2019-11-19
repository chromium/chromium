// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/content_verifier_test_utils.h"

#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/run_loop.h"
#include "content/public/test/test_utils.h"
#include "extensions/browser/external_install_info.h"
#include "extensions/browser/updater/extension_downloader.h"
#include "extensions/browser/updater/manifest_fetch_data.h"
#include "extensions/common/extension_urls.h"
#include "extensions/common/verifier_formats.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

namespace content_verifier_test {

DownloaderTestDelegate::DownloaderTestDelegate() {}
DownloaderTestDelegate::~DownloaderTestDelegate() {}

void DownloaderTestDelegate::AddResponse(const ExtensionId& extension_id,
                                         const std::string& version_string,
                                         const base::FilePath& crx_path) {
  responses_[extension_id] = std::make_pair(version_string, crx_path);
}

const std::vector<std::unique_ptr<ManifestFetchData>>&
DownloaderTestDelegate::requests() {
  return requests_;
}

void DownloaderTestDelegate::StartUpdateCheck(
    ExtensionDownloader* downloader,
    ExtensionDownloaderDelegate* delegate,
    std::unique_ptr<ManifestFetchData> fetch_data) {
  requests_.push_back(std::move(fetch_data));
  const ManifestFetchData* data = requests_.back().get();
  for (const auto& id : data->extension_ids()) {
    if (base::Contains(responses_, id)) {
      // We use PostTask here instead of calling OnExtensionDownloadFinished
      // immeditately, because the calling code isn't expecting a synchronous
      // response (in non-test situations there are at least 2 network
      // requests needed before a file could be returned).
      base::ThreadTaskRunnerHandle::Get()->PostTask(
          FROM_HERE,
          base::BindOnce(
              &ExtensionDownloaderDelegate::OnExtensionDownloadFinished,
              base::Unretained(delegate),
              CRXFileInfo(id, GetTestVerifierFormat(), responses_[id].second),
              false /* pass_file_ownership */, GURL(), responses_[id].first,
              ExtensionDownloaderDelegate::PingResult(), data->request_ids(),
              ExtensionDownloaderDelegate::InstallCallback()));
    }
  }
}

ForceInstallProvider::ForceInstallProvider(const ExtensionId& id) : id_(id) {}
ForceInstallProvider::~ForceInstallProvider() {}

std::string ForceInstallProvider::GetDebugPolicyProviderName() const {
  return "ForceInstallProvider";
}

bool ForceInstallProvider::UserMayModifySettings(const Extension* extension,
                                                 base::string16* error) const {
  return extension->id() != id_;
}

bool ForceInstallProvider::MustRemainEnabled(const Extension* extension,
                                             base::string16* error) const {
  return extension->id() == id_;
}

DelayTracker::DelayTracker()
    : action_(base::BindRepeating(&DelayTracker::ReinstallAction,
                                  base::Unretained(this))) {
  PolicyExtensionReinstaller::set_policy_reinstall_action_for_test(&action_);
}

DelayTracker::~DelayTracker() {
  PolicyExtensionReinstaller::set_policy_reinstall_action_for_test(nullptr);
}

const std::vector<base::TimeDelta>& DelayTracker::calls() {
  return calls_;
}

void DelayTracker::ReinstallAction(base::OnceClosure callback,
                                   base::TimeDelta delay) {
  saved_callback_ = std::move(callback);
  calls_.push_back(delay);
}

void DelayTracker::Proceed() {
  ASSERT_TRUE(saved_callback_);
  ASSERT_TRUE(!saved_callback_->is_null());
  // Run() will set |saved_callback_| again, so use a temporary: |callback|.
  base::OnceClosure callback = std::move(saved_callback_.value());
  saved_callback_.reset();
  std::move(callback).Run();
}

void DelayTracker::StopWatching() {
  PolicyExtensionReinstaller::set_policy_reinstall_action_for_test(nullptr);
}

}  // namespace content_verifier_test

}  // namespace extensions
