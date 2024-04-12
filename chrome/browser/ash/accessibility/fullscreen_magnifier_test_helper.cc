// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/accessibility/fullscreen_magnifier_test_helper.h"

#include "ash/accessibility/magnifier/fullscreen_magnifier_controller.h"
#include "ash/shell.h"
#include "base/functional/bind.h"
#include "chrome/browser/ash/accessibility/accessibility_manager.h"
#include "chrome/browser/ash/accessibility/accessibility_test_utils.h"
#include "chrome/browser/ash/accessibility/magnification_manager.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/accessibility_notification_waiter.h"
#include "extensions/browser/browsertest_util.h"
#include "extensions/browser/extension_host_test_helper.h"
#include "ui/accessibility/ax_mode.h"

namespace ash {

namespace {

FullscreenMagnifierController* GetFullscreenMagnifierController() {
  return Shell::Get()->fullscreen_magnifier_controller();
}

gfx::Rect GetViewPort() {
  return GetFullscreenMagnifierController()->GetViewportRect();
}

}  // namespace

// static
void FullscreenMagnifierTestHelper::WaitForMagnifierJSReady(Profile* profile) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  std::string script = base::StringPrintf(R"JS(
      (async function() {
        globalThis.accessibilityCommon.setFeatureLoadCallbackForTest(
            'magnifier', () => {
              globalThis.accessibilityCommon.magnifier_.setIsInitializingForTest(
                  false);
              chrome.test.sendScriptResult('ready');
            });
      })();
    )JS");
  base::Value result =
      extensions::browsertest_util::ExecuteScriptInBackgroundPage(
          profile, extension_misc::kAccessibilityCommonExtensionId, script);
  ASSERT_EQ("ready", result);
}

FullscreenMagnifierTestHelper::FullscreenMagnifierTestHelper(
    gfx::Point center_position_on_load)
    : center_position_on_load_(center_position_on_load) {
  AccessibilityManager::Get()->SetMagnifierBoundsObserverForTest(
      base::BindRepeating(
          &FullscreenMagnifierTestHelper::OnMagnifierBoundsChanged,
          weak_ptr_factory_.GetWeakPtr()));
}
FullscreenMagnifierTestHelper::~FullscreenMagnifierTestHelper() = default;

void FullscreenMagnifierTestHelper::LoadMagnifier(Profile* profile) {
  extensions::ExtensionHostTestHelper host_helper(
      profile, extension_misc::kAccessibilityCommonExtensionId);
  ASSERT_FALSE(MagnificationManager::Get()->IsMagnifierEnabled());
  MagnificationManager::Get()->SetMagnifierEnabled(true);

  // FullscreenMagnifierController moves the magnifier window with animation
  // when the magnifier is first enabled. It will move the mouse cursor
  // when the animation completes. Wait until the animation completes, so that
  // the mouse movement won't affect the position of magnifier window later.
  MagnifierAnimationWaiter magnifier_waiter(GetFullscreenMagnifierController());
  magnifier_waiter.Wait();
  host_helper.WaitForHostCompletedFirstLoad();

  // Start in a known location.
  MoveMagnifierWindow(center_position_on_load_.x(),
                      center_position_on_load_.y());
  ASSERT_EQ(GetViewPort().CenterPoint(), center_position_on_load_);

  WaitForMagnifierJSReady(profile);

  // Confirms that magnifier is enabled.
  ASSERT_TRUE(MagnificationManager::Get()->IsMagnifierEnabled());
  // Check default scale is as expected.
  ASSERT_EQ(2.0f, GetFullscreenMagnifierController()->GetScale());
}

void FullscreenMagnifierTestHelper::MoveMagnifierWindow(int x_center,
                                                        int y_center) {
  gfx::Rect bounds = GetViewPort();
  GetFullscreenMagnifierController()->MoveWindow(x_center - bounds.width() / 2,
                                                 y_center - bounds.height() / 2,
                                                 /*animate=*/false);
  WaitForMagnifierBoundsChangedTo(gfx::Point(x_center, y_center));
}

void FullscreenMagnifierTestHelper::WaitForMagnifierBoundsChanged() {
  base::RunLoop loop;
  bounds_changed_waiter_ = loop.QuitClosure();
  loop.Run();
}

void FullscreenMagnifierTestHelper::WaitForMagnifierBoundsChangedTo(
    gfx::Point center_point) {
  while (GetViewPort().CenterPoint() != center_point) {
    WaitForMagnifierBoundsChanged();
  }
}

void FullscreenMagnifierTestHelper::OnMagnifierBoundsChanged() {
  if (!bounds_changed_waiter_) {
    return;
  }

  std::move(bounds_changed_waiter_).Run();

  // Note: no need to wait for animation to get updated viewport. That can be
  // done separately if we need to check cursor changes.
}

}  // namespace ash
