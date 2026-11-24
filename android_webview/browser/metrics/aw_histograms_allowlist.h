// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_METRICS_AW_HISTOGRAMS_ALLOWLIST_H_
#define ANDROID_WEBVIEW_BROWSER_METRICS_AW_HISTOGRAMS_ALLOWLIST_H_

#include <stdint.h>

#include <unordered_set>

#include "base/no_destructor.h"

namespace android_webview {

// Keeps a list of which histograms to upload if histograms filtering is
// applied.
//
// The list contains histograms that will be sampled at 100%. This is not done
// for all histograms in order to preserve network usage and storage. See
// go/clank-webview-uma#histograms-allowlist-guidance for reasons to add your
// histogram to it and how to do it safely.
class AwHistogramsAllowlist {
 public:
  static AwHistogramsAllowlist* GetInstance();

  AwHistogramsAllowlist(const AwHistogramsAllowlist&) = delete;
  AwHistogramsAllowlist& operator=(const AwHistogramsAllowlist&) = delete;

  bool Contains(uint64_t hash) const;

 private:
  friend class base::NoDestructor<AwHistogramsAllowlist>;

  AwHistogramsAllowlist();
  ~AwHistogramsAllowlist();

  std::unordered_set<uint64_t> allowlist_hashes_;
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_METRICS_AW_HISTOGRAMS_ALLOWLIST_H_
