// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_CONTENT_VERIFIER_TEST_UTILS_H_
#define CHROME_BROWSER_EXTENSIONS_CONTENT_VERIFIER_TEST_UTILS_H_

#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/functional/callback_helpers.h"
#include "base/time/time.h"
#include "chrome/browser/extensions/corrupted_extension_reinstaller.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/browser/content_verifier/content_verifier.h"
#include "extensions/browser/content_verifier/content_verify_job.h"
#include "extensions/browser/external_provider_interface.h"
#include "extensions/browser/management_policy.h"
#include "extensions/browser/updater/extension_downloader_test_delegate.h"

namespace extensions {

class ExtensionDownloader;
class ExtensionDownloaderDelegate;

namespace content_verifier_test {

// This lets us intercept requests for update checks of extensions, and
// substitute a local file as a simulated response.
class DownloaderTestDelegate : public ExtensionDownloaderTestDelegate {
 public:
  DownloaderTestDelegate();

  DownloaderTestDelegate(const DownloaderTestDelegate&) = delete;
  DownloaderTestDelegate& operator=(const DownloaderTestDelegate&) = delete;

  ~DownloaderTestDelegate();

  // This makes it so that update check requests for |extension_id| will return
  // a downloaded file of |crx_path| that is claimed to have version
  // |version_string|.
  void AddResponse(const ExtensionId& extension_id,
                   const std::string& version_string,
                   const base::FilePath& crx_path);

  const std::vector<ExtensionDownloaderTask>& requests();

  // ExtensionDownloaderTestDelegate:
  void StartUpdateCheck(ExtensionDownloader* downloader,
                        ExtensionDownloaderDelegate* delegate,
                        std::vector<ExtensionDownloaderTask> tasks) override;

 private:
  // The requests we've received.
  std::vector<ExtensionDownloaderTask> requests_;

  // The prepared responses - this maps an extension id to a (version string,
  // crx file path) pair.
  std::map<ExtensionId, std::pair<base::Version, base::FilePath>> responses_;
};

// This lets us simulate a policy-installed extension being "force" installed;
// ie a user is not allowed to manually uninstall/disable it.
class ForceInstallProvider : public ManagementPolicy::Provider {
 public:
  explicit ForceInstallProvider(const ExtensionId& id);

  ForceInstallProvider(const ForceInstallProvider&) = delete;
  ForceInstallProvider& operator=(const ForceInstallProvider&) = delete;

  ~ForceInstallProvider() override;

  std::string GetDebugPolicyProviderName() const override;
  bool UserMayModifySettings(const Extension* extension,
                             std::u16string* error) const override;
  bool MustRemainEnabled(const Extension* extension,
                         std::u16string* error) const override;

 private:
  // The extension id we want to disallow uninstall/disable for.
  ExtensionId id_;
};

// A helper for intercepting the normal action that
// ChromeContentVerifierDelegate would take on discovering corruption, letting
// us track the delay for each consecutive reinstall.
class DelayTracker {
 public:
  DelayTracker();

  DelayTracker(const DelayTracker&) = delete;
  DelayTracker& operator=(const DelayTracker&) = delete;

  ~DelayTracker();

  const std::vector<base::TimeDelta>& calls();
  void ReinstallAction(base::OnceClosure callback, base::TimeDelta delay);
  void Proceed();
  void StopWatching();

 private:
  std::vector<base::TimeDelta> calls_;
  std::optional<base::OnceClosure> saved_callback_;
  CorruptedExtensionReinstaller::ReinstallCallback action_;
};

}  // namespace content_verifier_test

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_CONTENT_VERIFIER_TEST_UTILS_H_
