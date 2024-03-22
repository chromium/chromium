// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_HANDLERS_MULTI_SCREEN_CAPTURE_POLICY_HANDLER_H_
#define CHROME_BROWSER_ASH_POLICY_HANDLERS_MULTI_SCREEN_CAPTURE_POLICY_HANDLER_H_

#include "components/policy/core/browser/configuration_policy_handler.h"

namespace policy {

class MultiScreenCapturePolicyHandler : public policy::ListPolicyHandler {
 public:
  MultiScreenCapturePolicyHandler();

  MultiScreenCapturePolicyHandler(const MultiScreenCapturePolicyHandler&) =
      delete;
  MultiScreenCapturePolicyHandler& operator=(
      const MultiScreenCapturePolicyHandler&) = delete;

  ~MultiScreenCapturePolicyHandler() override;

 protected:
  // policy::ListPolicyHandler:
  bool CheckListEntry(const base::Value& value) override;
  void ApplyList(base::Value::List filtered_list, PrefValueMap* prefs) override;
};

}  // namespace policy

#endif  // CHROME_BROWSER_ASH_POLICY_HANDLERS_MULTI_SCREEN_CAPTURE_POLICY_HANDLER_H_
