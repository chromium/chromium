// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_TEST_SUPPORT_MOCK_GLIC_WINDOW_CONTROLLER_H_
#define CHROME_BROWSER_GLIC_TEST_SUPPORT_MOCK_GLIC_WINDOW_CONTROLLER_H_

#include "chrome/browser/glic/widget/glic_window_controller.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace glic {

class MockGlicWindowController
    : public testing::NiceMock<GlicWindowController> {
 public:
  MockGlicWindowController();
  ~MockGlicWindowController();

  MOCK_METHOD(void,
              Toggle,
              (BrowserWindowInterface*, bool, mojom::InvocationSource),
              (override));
  MOCK_METHOD(void, ShowAfterSignIn, (base::WeakPtr<Browser>), (override));
  MOCK_METHOD(void,
              ToggleWhenNotAlwaysDetached,
              (Browser*, bool, mojom::InvocationSource),
              (override));
  MOCK_METHOD(void, FocusIfOpen, (), (override));
  MOCK_METHOD(void, Attach, (), (override));
  MOCK_METHOD(void, Detach, (), (override));
  MOCK_METHOD(void, Shutdown, (), (override));
  MOCK_METHOD(void,
              Resize,
              (const gfx::Size&, base::TimeDelta, base::OnceClosure),
              (override));
  MOCK_METHOD(void, EnableDragResize, (bool), (override));
  MOCK_METHOD(gfx::Size, GetSize, (), (override));
  MOCK_METHOD(void,
              SetDraggableAreas,
              (const std::vector<gfx::Rect>&),
              (override));
  MOCK_METHOD(void, SetMinimumWidgetSize, (const gfx::Size&), (override));
  MOCK_METHOD(void, Close, (), (override));
  MOCK_METHOD(void, CloseWithReason, (views::Widget::ClosedReason), (override));
  MOCK_METHOD(void, ShowTitleBarContextMenuAt, (gfx::Point), (override));
  MOCK_METHOD(bool,
              ShouldStartDrag,
              (const gfx::Point&, const gfx::Point&),
              (override));
  MOCK_METHOD(void, HandleWindowDragWithOffset, (gfx::Vector2d), (override));
  MOCK_METHOD(const mojom::PanelState&, GetPanelState, (), (const, override));
  MOCK_METHOD(void, AddStateObserver, (StateObserver*), (override));
  MOCK_METHOD(void, RemoveStateObserver, (StateObserver*), (override));
  MOCK_METHOD(bool, IsActive, (), (override));
  MOCK_METHOD(bool, IsShowing, (), (const, override));
  MOCK_METHOD(bool, IsPanelOrFreShowing, (), (const, override));
  MOCK_METHOD(bool, IsAttached, (), (const, override));
  MOCK_METHOD(bool, IsDetached, (), (const, override));
  MOCK_METHOD(base::CallbackListSubscription,
              AddWindowActivationChangedCallback,
              (WindowActivationChangedCallback),
              (override));
  MOCK_METHOD(void, Preload, (), (override));
  MOCK_METHOD(void, PreloadFre, (), (override));
  MOCK_METHOD(void, Reload, (), (override));
  MOCK_METHOD(bool, IsWarmed, (), (const, override));
  MOCK_METHOD(GlicView*, GetGlicView, (), (override));
  MOCK_METHOD(base::WeakPtr<views::View>, GetGlicViewAsView, (), (override));
  MOCK_METHOD(GlicWidget*, GetGlicWidget, (), (override));
  MOCK_METHOD(content::WebContents*, GetFreWebContents, (), (override));
  MOCK_METHOD(Browser*, attached_browser, (), (override));
  MOCK_METHOD(State, state, (), (const, override));
  MOCK_METHOD(GlicFreController*, fre_controller, (), (override));
  MOCK_METHOD(GlicWindowAnimator*, window_animator, (), (override));
  MOCK_METHOD(Profile*, profile, (), (override));
  MOCK_METHOD(bool, IsDragging, (), (override));
  MOCK_METHOD(gfx::Rect, GetInitialBounds, (Browser*), (override));
  MOCK_METHOD(void, ShowDetachedForTesting, (), (override));
  MOCK_METHOD(void, SetPreviousPositionForTesting, (gfx::Point), (override));

  base::WeakPtr<GlicWindowController> GetWeakPtr() override {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  base::WeakPtrFactory<MockGlicWindowController> weak_ptr_factory_{this};
};
}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_TEST_SUPPORT_MOCK_GLIC_WINDOW_CONTROLLER_H_
