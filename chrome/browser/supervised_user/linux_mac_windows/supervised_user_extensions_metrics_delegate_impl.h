// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SUPERVISED_USER_LINUX_MAC_WINDOWS_SUPERVISED_USER_EXTENSIONS_METRICS_DELEGATE_IMPL_H_
#define CHROME_BROWSER_SUPERVISED_USER_LINUX_MAC_WINDOWS_SUPERVISED_USER_EXTENSIONS_METRICS_DELEGATE_IMPL_H_

#include "chrome/browser/profiles/profile.h"
#include "components/supervised_user/core/browser/supervised_user_metrics_service.h"
#include "extensions/browser/extension_registry.h"


class SupervisedUserExtensionsMetricsDelegateImpl
    : public supervised_user::SupervisedUserMetricsService::
          SupervisedUserMetricsServiceExtensionDelegate {
 public:
  // TODO (b/356824920) This should use the IdentityManager instead of the
  // profile and service derived from the profile.
  SupervisedUserExtensionsMetricsDelegateImpl(
      const extensions::ExtensionRegistry* extension_registry,
      Profile* profile);
  ~SupervisedUserExtensionsMetricsDelegateImpl() override;
  // SupervisedUserMetricsServiceExtensionDelegate implementation:
  bool RecordExtensionsMetrics() override;

 private:
  const raw_ptr<const extensions::ExtensionRegistry> extension_registry_;
  const raw_ptr<Profile> profile_;
};

#endif  // CHROME_BROWSER_SUPERVISED_USER_LINUX_MAC_WINDOWS_SUPERVISED_USER_EXTENSIONS_METRICS_DELEGATE_IMPL_H_
