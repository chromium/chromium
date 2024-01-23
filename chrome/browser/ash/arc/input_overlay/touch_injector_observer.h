// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_TOUCH_INJECTOR_OBSERVER_H_
#define CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_TOUCH_INJECTOR_OBSERVER_H_

#include "base/observer_list_types.h"

namespace arc::input_overlay {

class Action;

// Observer for UIs to update the content when the touch injector updates, adds,
// removes actions and changes action types.
class TouchInjectorObserver : public base::CheckedObserver {
 public:
  TouchInjectorObserver();

  virtual void OnActionAdded(Action& action) {}
  virtual void OnActionRemoved(const Action& action) {}
  // Once action type is changed, the original action is removed and
  // `new_action` with new type is added.
  virtual void OnActionTypeChanged(Action* action, Action* new_action) {}
  virtual void OnActionInputBindingUpdated(const Action& action) {}
  virtual void OnContentBoundsSizeChanged() {}
  virtual void OnActionNewStateRemoved(const Action& action) {}

 protected:
  ~TouchInjectorObserver() override;
};

}  // namespace arc::input_overlay

#endif  // CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_TOUCH_INJECTOR_OBSERVER_H_
