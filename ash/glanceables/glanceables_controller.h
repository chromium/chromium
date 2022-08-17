// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_GLANCEABLES_GLANCEABLES_CONTROLLER_H_
#define ASH_GLANCEABLES_GLANCEABLES_CONTROLLER_H_

#include <memory>

#include "ash/ash_export.h"

namespace views {
class Widget;
}  // namespace views

namespace ash {

class GlanceablesView;

// Controls the "welcome back" glanceables screen shown on login.
class ASH_EXPORT GlanceablesController {
 public:
  GlanceablesController();
  GlanceablesController(const GlanceablesController&) = delete;
  GlanceablesController& operator=(const GlanceablesController&) = delete;
  ~GlanceablesController();

  // Creates the glanceables widget and view.
  void CreateUi();

  // Destroys the glanceables widget and view.
  void DestroyUi();

  // Triggers a fetch of data from the server. This method is separate from
  // CreateUi() so we can avoid triggering server fetches in tests.
  void FetchData();

  views::Widget* widget_for_test() { return widget_.get(); }
  GlanceablesView* view_for_test() { return view_; }

 private:
  std::unique_ptr<views::Widget> widget_;
  GlanceablesView* view_ = nullptr;
};

}  // namespace ash

#endif  // ASH_GLANCEABLES_GLANCEABLES_CONTROLLER_H_
