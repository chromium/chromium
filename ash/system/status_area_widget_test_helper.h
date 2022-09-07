// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_STATUS_AREA_WIDGET_TEST_HELPER_H_
#define ASH_SYSTEM_STATUS_AREA_WIDGET_TEST_HELPER_H_

#include "ui/compositor/layer.h"

namespace ash {

enum class LoginStatus;
class StatusAreaWidget;

class StatusAreaWidgetTestHelper {
 public:
  StatusAreaWidgetTestHelper() = delete;
  StatusAreaWidgetTestHelper(const StatusAreaWidgetTestHelper&) = delete;
  StatusAreaWidgetTestHelper& operator=(const StatusAreaWidgetTestHelper&) =
      delete;

  static LoginStatus GetUserLoginStatus();

  // Returns the StatusAreaWidget that appears on the primary display.
  static StatusAreaWidget* GetStatusAreaWidget();

  // Returns the StatusAreaWidget that appears on the secondary display.
  static StatusAreaWidget* GetSecondaryStatusAreaWidget();

  // Waits until status area animations are over.
  static void WaitForAnimationEnd(StatusAreaWidget* status_area_widget);

  // Waits until one child view's layer animations are over.
  static void WaitForLayerAnimationEnd(ui::Layer* layer);
};

}  // namespace ash

#endif  // ASH_SYSTEM_STATUS_AREA_WIDGET_TEST_HELPER_H_
