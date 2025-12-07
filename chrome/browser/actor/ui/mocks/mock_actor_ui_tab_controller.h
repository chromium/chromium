// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_UI_MOCKS_MOCK_ACTOR_UI_TAB_CONTROLLER_H_
#define CHROME_BROWSER_ACTOR_UI_MOCKS_MOCK_ACTOR_UI_TAB_CONTROLLER_H_

#include "chrome/browser/actor/ui/actor_ui_tab_controller_interface.h"
#include "chrome/browser/ui/browser_window/test/mock_browser_window_interface.h"
#include "components/tabs/public/mock_tab_interface.h"
#include "components/tabs/public/tab_interface.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/base/unowned_user_data/unowned_user_data_host.h"

namespace actor::ui {

class MockActorUiTabController : public ActorUiTabControllerInterface {
 public:
  explicit MockActorUiTabController(tabs::MockTabInterface& mock_tab);
  ~MockActorUiTabController() override;

  // Sets up the default mock expectations to connect a mock tab to a mock
  // browser window and UnownedUserDataHost.
  static void SetupDefaultBrowserWindow(
      tabs::MockTabInterface& mock_tab,
      MockBrowserWindowInterface& mock_browser_window_interface,
      ::ui::UnownedUserDataHost& user_data_host);

  MOCK_METHOD(void,
              OnUiTabStateChange,
              (const UiTabState& ui_tab_state, UiResultCallback callback),
              (override));

  MOCK_METHOD(void, OnWebContentsAttached, (), (override));
  MOCK_METHOD(void, OnViewBoundsChanged, (), (override));

  MOCK_METHOD(base::WeakPtr<ActorUiTabControllerInterface>,
              GetWeakPtr,
              (),
              (override));

  MOCK_METHOD(void, SetActorTaskPaused, (), (override));

  MOCK_METHOD(void, SetActorTaskResume, (), (override));

  MOCK_METHOD(void,
              OnOverlayHoverStatusChanged,
              (bool is_hovering),
              (override));

  MOCK_METHOD(void, OnHandoffButtonHoverStatusChanged, (), (override));
  MOCK_METHOD(void, OnHandoffButtonFocusStatusChanged, (), (override));

  MOCK_METHOD(base::ScopedClosureRunner,
              RegisterHandoffButtonController,
              (HandoffButtonController * controller),
              (override));

  MOCK_METHOD(UiTabState, GetCurrentUiTabState, (), (const, override));

  MOCK_METHOD(void, OnImmersiveModeChanged, (), (override));

  using ActorOverlayStateChangeCallback =
      base::RepeatingCallback<void(bool, ActorOverlayState, base::OnceClosure)>;
  MOCK_METHOD(base::ScopedClosureRunner,
              RegisterActorOverlayStateChange,
              (ActorOverlayStateChangeCallback callback),
              (override));

  using ActorTabIndicatorStateChangedCallback =
      base::RepeatingCallback<void(TabIndicatorStatus)>;
  MOCK_METHOD(base::ScopedClosureRunner,
              RegisterActorTabIndicatorStateChangedCallback,
              (ActorTabIndicatorStateChangedCallback callback),
              (override));

  using ActorOverlayBackgroundChangeCallback =
      base::RepeatingCallback<void(bool)>;
  MOCK_METHOD(base::ScopedClosureRunner,
              RegisterActorOverlayBackgroundChange,
              (ActorOverlayBackgroundChangeCallback callback),
              (override));

 private:
  base::WeakPtrFactory<MockActorUiTabController> weak_factory_{this};
};

}  // namespace actor::ui

#endif  // CHROME_BROWSER_ACTOR_UI_MOCKS_MOCK_ACTOR_UI_TAB_CONTROLLER_H_
