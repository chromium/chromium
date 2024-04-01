// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/input_overlay/test/view_test_base.h"

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/window_properties.h"
#include "base/json/json_reader.h"
#include "base/test/bind.h"
#include "chrome/browser/ash/arc/input_overlay/actions/action.h"
#include "chrome/browser/ash/arc/input_overlay/display_overlay_controller.h"
#include "chrome/browser/ash/arc/input_overlay/test/test_utils.h"
#include "chrome/browser/ash/arc/input_overlay/touch_injector.h"
#include "chrome/browser/ash/arc/input_overlay/ui/action_view.h"
#include "chrome/browser/ash/arc/input_overlay/ui/input_mapping_view.h"
#include "chrome/browser/ash/arc/input_overlay/util.h"
#include "ui/views/view.h"

namespace arc::input_overlay {

namespace {
constexpr const char kValidJson[] =
    R"json({
      "move": [
        {
          "id": 0,
          "input_sources": [
            "keyboard"
          ],
          "name": "Virtual Joystick",
          "keys": [
            "KeyW",
            "KeyA",
            "KeyS",
            "KeyD"
          ],
          "location": [
            {
              "type": "position",
              "anchor": [
                0,
                0
              ],
              "anchor_to_target": [
                0.5,
                0.5
              ]
            }
          ]
        }
      ],
      "tap": [
        {
          "id": 1,
          "input_sources": [
            "keyboard"
          ],
          "name": "Fight",
          "key": "Space",
          "location": [
            {
              "type": "position",
              "anchor": [
                0,
                0
              ],
              "anchor_to_target": [
                0.5,
                0.5
              ]
            },
            {
              "type": "position",
              "anchor": [
                0,
                0
              ],
              "anchor_to_target": [
                0.3,
                0.3
              ]
            }
          ]
        }
      ]
    })json";
}  // namespace

ViewTestBase::ViewTestBase()
    : views::ViewsTestBase(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {
}

ViewTestBase::~ViewTestBase() = default;

void ViewTestBase::SetDisplayMode(DisplayMode display_mode) {
  input_mapping_view_->SetDisplayMode(display_mode);
}

void ViewTestBase::SetUp() {
  views::ViewsTestBase::SetUp();
  Init();
}

void ViewTestBase::TearDown() {
  input_mapping_view_.reset();
  display_overlay_controller_.reset();
  touch_injector_.reset();
  widget_.reset();
  views::ViewsTestBase::TearDown();
}

void ViewTestBase::Init() {
  scoped_feature_list_.InitAndDisableFeature(ash::features::kGameDashboard);
  root_window()->SetBounds(gfx::Rect(1000, 800));
  widget_ = CreateArcWindow(root_window(), gfx::Rect(200, 100, 600, 400));
  touch_injector_ = std::make_unique<TouchInjector>(
      widget_->GetNativeWindow(),
      *widget_->GetNativeWindow()->GetProperty(ash::kArcPackageNameKey),
      base::BindLambdaForTesting(
          [&](std::unique_ptr<AppDataProto>, std::string) {}));
  touch_injector_->ParseActions(
      base::JSONReader::ReadAndReturnValueWithError(kValidJson)
          .value()
          .TakeDict());
  touch_injector_->RegisterEventRewriter();
  display_overlay_controller_ = std::make_unique<DisplayOverlayController>(
      touch_injector_.get(), /*first_launch=*/false);
  input_mapping_view_ =
      std::make_unique<InputMappingView>(display_overlay_controller_.get());

  const auto& actions = touch_injector_->actions();
  DCHECK_EQ(2u, actions.size());
  // ActionTap is added first.
  tap_action_ = actions[0].get();
  tap_action_view_ = tap_action_->action_view();
  move_action_ = actions[1].get();
  move_action_view_ = move_action_->action_view();
  SetDisplayMode(DisplayMode::kView);
}

}  // namespace arc::input_overlay
