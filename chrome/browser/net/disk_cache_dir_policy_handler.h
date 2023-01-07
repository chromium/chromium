// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NET_DISK_CACHE_DIR_POLICY_HANDLER_H_
#define CHROME_BROWSER_NET_DISK_CACHE_DIR_POLICY_HANDLER_H_

#include "components/policy/core/browser/configuration_policy_handler.h"

namespace policy {

// ConfigurationPolicyHandler for the DiskCacheDir policy.
class DiskCacheDirPolicyHandler : public TypeCheckingPolicyHandler {
 public:
  DiskCacheDirPolicyHandler();

  DiskCacheDirPolicyHandler(const DiskCacheDirPolicyHandler&) = delete;
  DiskCacheDirPolicyHandler& operator=(const DiskCacheDirPolicyHandler&) =
      delete;

  ~DiskCacheDirPolicyHandler() override;

  // ConfigurationPolicyHandler methods:
  void ApplyPolicySettings(const PolicyMap& policies,
                           PrefValueMap* prefs) override;
};

}  // namespace policy

#endif  // CHROME_BROWSER_NET_DISK_CACHE_DIR_POLICY_HANDLER_H_
