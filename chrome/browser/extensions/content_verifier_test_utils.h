// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_CONTENT_VERIFIER_TEST_UTILS_H_
#define CHROME_BROWSER_EXTENSIONS_CONTENT_VERIFIER_TEST_UTILS_H_

#include <list>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/callback_helpers.h"
#include "base/files/file_path.h"
#include "chrome/browser/extensions/policy_extension_reinstaller.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/browser/content_verifier.h"
#include "extensions/browser/content_verify_job.h"
#include "extensions/browser/external_provider_interface.h"
#include "extensions/browser/management_policy.h"
#include "extensions/browser/updater/extension_downloader_test_delegate.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace extensions {

class ExtensionDownloader;
class ExtensionDownloaderDelegate;
class ManifestFetchData;

namespace content_verifier_test {

// This lets us intercept requests for update checks of extensions, and
// substitute a local file as a simulated response.
class DownloaderTestDelegate : public ExtensionDownloaderTestDelegate {
 public:
  DownloaderTestDelegate();
  ~DownloaderTestDelegate();

  // This makes it so that update check requests for |extension_id| will return
  // a downloaded file of |crx_path| that is claimed to have version
  // |version_string|.
  void AddResponse(const ExtensionId& extension_id,
                   const std::string& version_string,
                   const base::FilePath& crx_path);

  const std::vector<std::unique_ptr<ManifestFetchData>>& requests();

  // ExtensionDownloaderTestDelegate:
  void StartUpdateCheck(ExtensionDownloader* downloader,
                        ExtensionDownloaderDelegate* delegate,
                        std::unique_ptr<ManifestFetchData> fetch_data) override;

 private:
  // The requests we've received.
  std::vector<std::unique_ptr<ManifestFetchData>> requests_;

  // The prepared responses - this maps an extension id to a (version string,
  // crx file path) pair.
  std::map<ExtensionId, std::pair<base::Version, base::FilePath>> responses_;

  DISALLOW_COPY_AND_ASSIGN(DownloaderTestDelegate);
};

// This lets us simulate a policy-installed extension being "force" installed;
// ie a user is not allowed to manually uninstall/disable it.
class ForceInstallProvider : public ManagementPolicy::Provider {
 public:
  explicit ForceInstallProvider(const ExtensionId& id);
  ~ForceInstallProvider() override;

  std::string GetDebugPolicyProviderName() const override;
  bool UserMayModifySettings(const Extension* extension,
                             std::u16string* error) const override;
  bool MustRemainEnabled(const Extension* extension,
                         std::u16string* error) const override;

 private:
  // The extension id we want to disallow uninstall/disable for.
  ExtensionId id_;

  DISALLOW_COPY_AND_ASSIGN(ForceInstallProvider);
};

// A helper for intercepting the normal action that
// ChromeContentVerifierDelegate would take on discovering corruption, letting
// us track the delay for each consecutive reinstall.
class DelayTracker {
 public:
  DelayTracker();

  ~DelayTracker();

  const std::vector<base::TimeDelta>& calls();
  void ReinstallAction(base::OnceClosure callback, base::TimeDelta delay);
  void Proceed();
  void StopWatching();

 private:
  std::vector<base::TimeDelta> calls_;
  absl::optional<base::OnceClosure> saved_callback_;
  PolicyExtensionReinstaller::ReinstallCallback action_;

  DISALLOW_COPY_AND_ASSIGN(DelayTracker);
};

}  // namespace content_verifier_test

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_CONTENT_VERIFIER_TEST_UTILS_H_
