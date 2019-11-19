// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TOUCH_TO_FILL_TOUCH_TO_FILL_VIEW_FACTORY_H_
#define CHROME_BROWSER_TOUCH_TO_FILL_TOUCH_TO_FILL_VIEW_FACTORY_H_

#include <memory>

class TouchToFillView;
class TouchToFillController;

class TouchToFillViewFactory {
 public:
  static std::unique_ptr<TouchToFillView> Create(
      TouchToFillController* controller);
};

#endif  // CHROME_BROWSER_TOUCH_TO_FILL_TOUCH_TO_FILL_VIEW_FACTORY_H_
