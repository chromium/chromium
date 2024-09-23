// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_SAFE_BROWSING_AW_SAFE_BROWSING_ALLOWLIST_MANAGER_H_
#define ANDROID_WEBVIEW_BROWSER_SAFE_BROWSING_AW_SAFE_BROWSING_ALLOWLIST_MANAGER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/functional/callback_forward.h"
#include "base/memory/ref_counted.h"
#include "base/observer_list.h"
#include "url/gurl.h"

namespace base {
class SequencedTaskRunner;
}  // namespace base

namespace android_webview {
struct TrieNode;

class AwSafeBrowsingAllowlistManager;

class AwSafeBrowsingAllowlistSetObserver : public base::CheckedObserver {
 public:
  explicit AwSafeBrowsingAllowlistSetObserver(
      AwSafeBrowsingAllowlistManager* manager);

  virtual void OnSafeBrowsingAllowListSet() = 0;

  ~AwSafeBrowsingAllowlistSetObserver() override;

 private:
  // AwSafeBrowsingAllowlistManager has static singleton lifetime so raw_ptr
  // will be fine.
  raw_ptr<AwSafeBrowsingAllowlistManager> manager_;
};

// This class tracks the allowlisting policies for Safebrowsing. The class
// interacts with UI thread, where the allowlist is set, and then checks
// for the URLs in IO thread. The allowed entries are not checked for
// Safebrowsing.
//
// The class must be constructed on the UI thread.
//
// Update allowlist tasks from the UI thread are post to the IO thread.
//
// Encoding and the allowlisting rules:
//    The allowlist is set in Java and plumbed to native through JNI, making
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
// policy format:
//      https://www.chromium.org/administrators/url-blocklist-filter-format
//
// The expected number of entries on the list should be 100s at most, however
// the size is not enforced here. The list size can be enforced at
// Java level if necessary.
//
// Lifetime: Singleton
class AwSafeBrowsingAllowlistManager {
 public:
  // Must be constructed on the UI thread.
  // |background_task_runner| is used to build the filter list in a background
  // thread.
  // |io_task_runner| must be backed by the IO thread.
  AwSafeBrowsingAllowlistManager(
      const scoped_refptr<base::SequencedTaskRunner>& background_task_runner,
      const scoped_refptr<base::SequencedTaskRunner>& io_task_runner);

  AwSafeBrowsingAllowlistManager(const AwSafeBrowsingAllowlistManager&) =
      delete;
  AwSafeBrowsingAllowlistManager& operator=(
      const AwSafeBrowsingAllowlistManager&) = delete;

  virtual ~AwSafeBrowsingAllowlistManager();

  // Returns true if |url| is allowed by the current allowlist. Must be
  // called from the IO thread.
  bool IsUrlAllowed(const GURL& url) const;

  // Replace the current host allowlist with a new one.
  void SetAllowlistOnUIThread(std::vector<std::string>&& rules,
                              base::OnceCallback<void(bool)> callback);

  void RegisterAllowlistSetObserver(
      AwSafeBrowsingAllowlistSetObserver* observer);

  void RemoveAllowlistSetObserver(AwSafeBrowsingAllowlistSetObserver* observer);

 private:
  // Builds allowlist on background thread.
  void BuildAllowlist(const std::vector<std::string>& rules,
                      base::OnceCallback<void(bool)> callback);
  // Replaces the current allowlist. Must be called on the IO thread.
  void SetAllowlist(std::unique_ptr<TrieNode> allowlist);

  scoped_refptr<base::SequencedTaskRunner> background_task_runner_;
  scoped_refptr<base::SequencedTaskRunner> io_task_runner_;
  scoped_refptr<base::SequencedTaskRunner> ui_task_runner_;

  base::ObserverList<AwSafeBrowsingAllowlistSetObserver>
      allowlist_set_observers_;

  std::unique_ptr<TrieNode> allowlist_;
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_SAFE_BROWSING_AW_SAFE_BROWSING_ALLOWLIST_MANAGER_H_
