// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_SAFE_BROWSING_AW_SAFE_BROWSING_WHITELIST_MANAGER_H_
#define ANDROID_WEBVIEW_BROWSER_SAFE_BROWSING_AW_SAFE_BROWSING_WHITELIST_MANAGER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/memory/ref_counted.h"
#include "url/gurl.h"

namespace base {
class SequencedTaskRunner;
}  // namespace base

namespace android_webview {
struct TrieNode;

// This class tracks the whitelisting policies for Safebrowsing. The class
// interacts with UI thread, where the whitelist is set, and then checks
// for the URLs in IO thread. The whitelisted entries are not checked for
// Safebrowsing.
//
// The class must be constructed on the UI thread.
//
// Update whitelist tasks from the UI thread are post to the IO thread.
//
// Encoding and the whitelisting rules:
//    The whitelist is set in Java and plumbed to native through JNI, making
//  them UTF-8 encoded wide strings.
//
// Each rule should take one of these:
//    HOSTNAME
//   .HOSTNAME
//   IPV4_LITERAL
//   IPV6_LITERAL_WITH_BRACKETS
//
// All other rules, including wildcards, are invalid.
//
// The hostname with a leading dot means an exact match, otherwise subdomains
// are also matched. This particular rule is similar to admiministration
// blacklist policy format:
//      https://www.chromium.org/administrators/url-blacklist-filter-format
//
// The expected number of entries on the list should be 100s at most, however
// the size is not enforced here. The list size can be enforced at
// Java level if necessary.
//
class AwSafeBrowsingWhitelistManager {
 public:
  // Must be constructed on the UI thread.
  // |background_task_runner| is used to build the filter list in a background
  // thread.
  // |io_task_runner| must be backed by the IO thread.
  AwSafeBrowsingWhitelistManager(
      const scoped_refptr<base::SequencedTaskRunner>& background_task_runner,
      const scoped_refptr<base::SequencedTaskRunner>& io_task_runner);
  virtual ~AwSafeBrowsingWhitelistManager();

  // Returns true if |url| is whitelisted by the current whitelist. Must be
  // called from the IO thread.
  bool IsURLWhitelisted(const GURL& url) const;

  // Replace the current host whitelist with a new one.
  void SetWhitelistOnUIThread(std::vector<std::string>&& rules,
                              base::OnceCallback<void(bool)> callback);

 private:
  // Builds whitelist on background thread.
  void BuildWhitelist(const std::vector<std::string>& rules,
                      base::OnceCallback<void(bool)> callback);
  // Replaces the current whitelist. Must be called on the IO thread.
  void SetWhitelist(std::unique_ptr<TrieNode> whitelist);

  scoped_refptr<base::SequencedTaskRunner> background_task_runner_;
  scoped_refptr<base::SequencedTaskRunner> io_task_runner_;
  scoped_refptr<base::SequencedTaskRunner> ui_task_runner_;

  std::unique_ptr<TrieNode> whitelist_;

  DISALLOW_COPY_AND_ASSIGN(AwSafeBrowsingWhitelistManager);
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_SAFE_BROWSING_AW_SAFE_BROWSING_WHITELIST_MANAGER_H_
