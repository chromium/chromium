// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_TEST_VIEW_TEST_BASE_H_
#define CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_TEST_VIEW_TEST_BASE_H_

#include <memory>
#include <optional>

#include "base/memory/raw_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ash/arc/input_overlay/constants.h"
#include "ui/gfx/geometry/point.h"
#include "ui/views/test/views_test_base.h"

namespace views {
class Widget;
}

namespace arc::input_overlay {

class Action;
class ActionView;
class DisplayOverlayController;
class InputMappingView;
class TouchInjector;

class ViewTestBase : public views::ViewsTestBase {
 public:
  ViewTestBase();
  ~ViewTestBase() override;

  void SetDisplayMode(DisplayMode display_mode);

  // test::Test:
  void SetUp() override;
  void TearDown() override;

 protected:
  std::unique_ptr<TouchInjector> touch_injector_;
  std::unique_ptr<DisplayOverlayController> display_overlay_controller_;
  std::unique_ptr<InputMappingView> input_mapping_view_;
  std::unique_ptr<views::Widget> widget_;

  raw_ptr<ActionView, DanglingUntriaged> move_action_view_;
  raw_ptr<ActionView, DanglingUntriaged> tap_action_view_;
  raw_ptr<Action, DanglingUntriaged> move_action_;
  raw_ptr<Action, DanglingUntriaged> tap_action_;
  gfx::Point root_location_;
  gfx::Point local_location_;

 private:
  void Init();

  base::test::ScopedFeatureList scoped_feature_list_;
};

}  // namespace arc::input_overlay

#endif  // CHROME_BROWSER_ASH_ARC_INPUT_OVERLAY_TEST_VIEW_TEST_BASE_H_
