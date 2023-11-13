// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TOUCH_TO_FILL_PASSWORD_MANAGER_TOUCH_TO_FILL_VIEW_FACTORY_H_
#define CHROME_BROWSER_TOUCH_TO_FILL_PASSWORD_MANAGER_TOUCH_TO_FILL_VIEW_FACTORY_H_

#include <memory>

class TouchToFillView;
class TouchToFillController;

class TouchToFillViewFactory {
 public:
  static std::unique_ptr<TouchToFillView> Create(
      TouchToFillController* controller);
};

#endif  // CHROME_BROWSER_TOUCH_TO_FILL_PASSWORD_MANAGER_TOUCH_TO_FILL_VIEW_FACTORY_H_
