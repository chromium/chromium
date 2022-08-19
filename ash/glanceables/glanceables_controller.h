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

class GlanceablesDelegate;
class GlanceablesView;

// Controls the "welcome back" glanceables screen shown on login.
class ASH_EXPORT GlanceablesController {
 public:
  explicit GlanceablesController(std::unique_ptr<GlanceablesDelegate> delegate);
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

  // Triggers a session restore.
  void RestoreSession();

 private:
  friend class GlanceablesTest;

  std::unique_ptr<GlanceablesDelegate> delegate_;
  std::unique_ptr<views::Widget> widget_;
  GlanceablesView* view_ = nullptr;
};

}  // namespace ash

#endif  // ASH_GLANCEABLES_GLANCEABLES_CONTROLLER_H_
