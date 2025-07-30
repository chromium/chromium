// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_UI_MOCK_ACTOR_OVERLAY_VIEW_CONTROLLER_H_
#define CHROME_BROWSER_ACTOR_UI_MOCK_ACTOR_OVERLAY_VIEW_CONTROLLER_H_

#include "chrome/browser/actor/ui/actor_overlay_view_controller.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace actor::ui {

// A mock class for ActorOverlayViewController.
class MockActorOverlayViewController : public ActorOverlayViewController {
 public:
  explicit MockActorOverlayViewController(tabs::TabInterface& tab_interface);
  ~MockActorOverlayViewController() override;

  MOCK_METHOD(void,
              BindOverlay,
              (mojo::PendingReceiver<mojom::ActorOverlayPageHandler> receiver),
              (override));
  MOCK_METHOD(ActorUiTabControllerInterface*, GetTabController, (), (override));
  MOCK_METHOD(void,
              UpdateState,
              (const ActorOverlayState& state, bool is_visible),
              (override));
  MOCK_METHOD(void,
              SetWindowController,
              (ActorOverlayWindowController * controller),
              (override));
  MOCK_METHOD(void, NullifyWebView, (), (override));
  MOCK_METHOD(void, OnHoverStatusChanged, (bool is_hovering), (override));
};

}  // namespace actor::ui

#endif  // CHROME_BROWSER_ACTOR_UI_MOCK_ACTOR_OVERLAY_VIEW_CONTROLLER_H_
