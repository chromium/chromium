// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_DATA_CONTROLS_CONDITION_H_
#define CHROME_BROWSER_ENTERPRISE_DATA_CONTROLS_CONDITION_H_

#include "chrome/browser/enterprise/data_controls/action_context.h"

namespace data_controls {

// Interface for a generic condition to be evaluated by a Data Controls policy.
class Condition {
 public:
  virtual bool IsTriggered(const ActionContext& action_context) const = 0;
};

}  // namespace data_controls

#endif  // CHROME_BROWSER_ENTERPRISE_DATA_CONTROLS_CONDITION_H_
