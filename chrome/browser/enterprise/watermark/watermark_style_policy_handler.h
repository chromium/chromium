// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_WATERMARK_WATERMARK_STYLE_POLICY_HANDLER_H_
#define CHROME_BROWSER_ENTERPRISE_WATERMARK_WATERMARK_STYLE_POLICY_HANDLER_H_

#include "components/policy/core/browser/configuration_policy_handler.h"

class WatermarkStylePolicyHandler : public policy::CloudOnlyPolicyHandler {
 public:
  explicit WatermarkStylePolicyHandler(policy::Schema schema);
  WatermarkStylePolicyHandler(WatermarkStylePolicyHandler&) = delete;
  WatermarkStylePolicyHandler& operator=(WatermarkStylePolicyHandler&) = delete;
  ~WatermarkStylePolicyHandler() override;

  // policy::CloudOnlyPolicyHandler:
  void ApplyPolicySettings(const policy::PolicyMap& policies,
                           PrefValueMap* prefs) override;
};

#endif  // CHROME_BROWSER_ENTERPRISE_WATERMARK_WATERMARK_STYLE_POLICY_HANDLER_H_
