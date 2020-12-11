// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_CHROME_CONTENT_VERIFIER_DELEGATE_H_
#define CHROME_BROWSER_EXTENSIONS_CHROME_CONTENT_VERIFIER_DELEGATE_H_

#include <map>
#include <memory>
#include <set>
#include <string>

#include "base/macros.h"
#include "base/optional.h"
#include "extensions/browser/content_verifier_delegate.h"

namespace content {
class BrowserContext;
}

namespace net {
class BackoffEntry;
}

namespace extensions {

class PolicyExtensionReinstaller;

class ChromeContentVerifierDelegate : public ContentVerifierDelegate {
 public:
  // Helper struct to encapsulate information we need to know about one
  // extension to make decisions about how to verify it and what actions should
  // be taken upon failures.
  struct VerifyInfo {
    // Note that it is important for these to appear in increasing "severity"
    // order, because we use this to let command line flags increase, but not
    // decrease, the mode you're running in compared to the experiment group.
    enum class Mode {
      // Do not try to fetch content hashes if they are missing, and do not
      // enforce them if they are present.
      NONE = 0,

      // If content hashes are missing, try to fetch them, but do not enforce.
      BOOTSTRAP,

      // If hashes are present, enforce them. If they are missing, try to fetch
      // them.
      ENFORCE,

      // Treat the absence of hashes the same as a verification failure.
      ENFORCE_STRICT
    };

    VerifyInfo(Mode mode, bool is_from_webstore, bool should_repair);

    // Verification mode for the extension.
    const Mode mode;

    // Whether the extension is from Chrome Web Store or not.
    const bool is_from_webstore;

    // Whether the extension should be automatically repaired in case of
    // corruption.
    const bool should_repair;
  };

  static VerifyInfo::Mode GetDefaultMode();
  static void SetDefaultModeForTesting(base::Optional<VerifyInfo::Mode> mode);

  explicit ChromeContentVerifierDelegate(content::BrowserContext* context);

  ~ChromeContentVerifierDelegate() override;

  // ContentVerifierDelegate:
  VerifierSourceType GetVerifierSourceType(const Extension& extension) override;
  ContentVerifierKey GetPublicKey() override;
  GURL GetSignatureFetchUrl(const std::string& extension_id,
                            const base::Version& version) override;
  std::set<base::FilePath> GetBrowserImagePaths(
      const extensions::Extension* extension) override;
  void VerifyFailed(const std::string& extension_id,
                    ContentVerifyJob::FailureReason reason) override;
  void Shutdown() override;

 private:
  // Returns true iff |extension| is considered extension from Chrome Web Store
  // (and therefore signed hashes may be used for its content verification).
  static bool IsFromWebstore(const Extension& extension);

  // Returns information needed for content verification of |extension|.
  VerifyInfo GetVerifyInfo(const Extension& extension) const;

  content::BrowserContext* context_;
  VerifyInfo::Mode default_mode_;

  // This maps an extension id to a backoff entry for slowing down
  // redownload/reinstall of corrupt policy extensions if it keeps happening
  // in a loop (eg crbug.com/661738).
  std::map<std::string, std::unique_ptr<net::BackoffEntry>>
      policy_reinstall_backoff_;

  // For reporting metrics in BOOTSTRAP mode, when an extension would be
  // disabled if content verification was in ENFORCE mode.
  std::set<std::string> would_be_disabled_ids_;

  // For reporting metrics about extensions without hashes, which we want to
  // reinstall in the future. See https://crbug.com/958794#c22 for details.
  std::set<std::string> would_be_reinstalled_ids_;

  std::unique_ptr<PolicyExtensionReinstaller> policy_extension_reinstaller_;

  DISALLOW_COPY_AND_ASSIGN(ChromeContentVerifierDelegate);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_CHROME_CONTENT_VERIFIER_DELEGATE_H_
