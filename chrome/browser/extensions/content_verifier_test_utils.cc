// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/content_verifier_test_utils.h"

#include <utility>

#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
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
  responses_[extension_id] =
      std::make_pair(base::Version(version_string), crx_path);
}

const std::vector<ExtensionDownloaderTask>& DownloaderTestDelegate::requests() {
  return requests_;
}

void DownloaderTestDelegate::StartUpdateCheck(
    ExtensionDownloader* downloader,
    ExtensionDownloaderDelegate* delegate,
    std::vector<ExtensionDownloaderTask> tasks) {
  ExtensionIdSet extension_ids;
  std::set<int> request_ids;
  for (ExtensionDownloaderTask& task : tasks) {
    extension_ids.insert(task.id);
    request_ids.insert(task.request_id);
  }
  for (ExtensionDownloaderTask& task : tasks)
    requests_.push_back(std::move(task));
  for (const auto& id : extension_ids) {
    if (base::Contains(responses_, id)) {
      CRXFileInfo crx_info(responses_[id].second, GetTestVerifierFormat());
      crx_info.extension_id = id;
      crx_info.expected_version = responses_[id].first;
      // We use PostTask here instead of calling OnExtensionDownloadFinished
      // immeditately, because the calling code isn't expecting a synchronous
      // response (in non-test situations there are at least 2 network
      // requests needed before a file could be returned).
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE,
          base::BindOnce(
              &ExtensionDownloaderDelegate::OnExtensionDownloadFinished,
              base::Unretained(delegate), crx_info,
              false /* pass_file_ownership */, GURL(),
              ExtensionDownloaderDelegate::PingResult(), request_ids,
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
                                                 std::u16string* error) const {
  return extension->id() != id_;
}

bool ForceInstallProvider::MustRemainEnabled(const Extension* extension,
                                             std::u16string* error) const {
  return extension->id() == id_;
}

DelayTracker::DelayTracker()
    : action_(base::BindRepeating(&DelayTracker::ReinstallAction,
                                  base::Unretained(this))) {
  CorruptedExtensionReinstaller::set_reinstall_action_for_test(&action_);
}

DelayTracker::~DelayTracker() {
  CorruptedExtensionReinstaller::set_reinstall_action_for_test(nullptr);
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
  CorruptedExtensionReinstaller::set_reinstall_action_for_test(nullptr);
}

}  // namespace content_verifier_test

}  // namespace extensions
