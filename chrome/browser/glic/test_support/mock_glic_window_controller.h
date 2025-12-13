// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_TEST_SUPPORT_MOCK_GLIC_WINDOW_CONTROLLER_H_
#define CHROME_BROWSER_GLIC_TEST_SUPPORT_MOCK_GLIC_WINDOW_CONTROLLER_H_

#include "chrome/browser/glic/widget/glic_view.h"
#include "chrome/browser/glic/widget/glic_window_controller.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkRegion.h"

namespace glic {

class MockGlicWindowController
    : public testing::NiceMock<GlicWindowControllerInterface> {
 public:
  MockGlicWindowController();
  ~MockGlicWindowController();

  MOCK_METHOD(HostManager&, host_manager, (), (override));
  MOCK_METHOD(std::vector<GlicInstance*>, GetInstances, (), (override));
  MOCK_METHOD(GlicInstance*,
              GetInstanceForTab,
              (const tabs::TabInterface*),
              (const, override));

  MOCK_METHOD(void,
              Toggle,
              (BrowserWindowInterface*,
               bool,
               mojom::InvocationSource,
               std::optional<std::string>),
              (override));
  MOCK_METHOD(void, ShowAfterSignIn, (base::WeakPtr<Browser>), (override));
  MOCK_METHOD(void, Attach, (), ());
  MOCK_METHOD(void, Detach, (), ());
  MOCK_METHOD(void, Shutdown, (), (override));
  MOCK_METHOD(void,
              Resize,
              (const gfx::Size&, base::TimeDelta, base::OnceClosure),
              ());
  MOCK_METHOD(void, EnableDragResize, (bool), ());
  MOCK_METHOD(void, MaybeSetWidgetCanResize, (), (override));
  MOCK_METHOD(gfx::Size, GetPanelSize, (), (override));
  MOCK_METHOD(void, SetDraggableAreas, (const std::vector<gfx::Rect>&), ());
  MOCK_METHOD(void, SetDraggableRegion, (const SkRegion&), ());
  MOCK_METHOD(void, SetMinimumWidgetSize, (const gfx::Size&), ());
  MOCK_METHOD(void, Close, (), (override));
  MOCK_METHOD(void,
              CloseInstanceWithFrame,
              (content::RenderFrameHost * render_frame_host),
              (override));
  MOCK_METHOD(void,
              CloseAndShutdownInstanceWithFrame,
              (content::RenderFrameHost * render_frame_host),
              (override));
  MOCK_METHOD(mojom::PanelState, GetPanelState, (), (override));
  MOCK_METHOD(void, AddStateObserver, (StateObserver*), (override));
  MOCK_METHOD(void, RemoveStateObserver, (StateObserver*), (override));
  MOCK_METHOD(bool, IsActive, (), (override));
  MOCK_METHOD(bool, IsShowing, (), (const));
  MOCK_METHOD(bool, IsAttached, (), (override));
  MOCK_METHOD(bool, IsDetached, (), (const, override));
  MOCK_METHOD(bool,
              IsPanelShowingForBrowser,
              (const BrowserWindowInterface&),
              (const, override));
  MOCK_METHOD(base::CallbackListSubscription,
              AddWindowActivationChangedCallback,
              (WindowActivationChangedCallback),
              (override));
  MOCK_METHOD(base::CallbackListSubscription,
              AddGlobalShowHideCallback,
              (base::RepeatingClosure),
              (override));
  MOCK_METHOD(void, Preload, (), (override));
  MOCK_METHOD(void,
              Reload,
              (content::RenderFrameHost * render_frame_host),
              (override));
  MOCK_METHOD(bool, IsWarmed, (), (const, override));
  MOCK_METHOD(GlicWidget*, GetGlicWidget, (), (const, override));
  MOCK_METHOD(Browser*, attached_browser, (), (override));
  MOCK_METHOD(State, state, (), (const, override));
  MOCK_METHOD(Profile*, profile, (), (override));
  MOCK_METHOD(gfx::Rect, GetInitialBounds, (Browser*), (override));
  MOCK_METHOD(void, ShowDetachedForTesting, (), (override));
  MOCK_METHOD(void, SetPreviousPositionForTesting, (gfx::Point), (override));
  MOCK_METHOD(std::unique_ptr<views::View>,
              CreateViewForSidePanel,
              (tabs::TabInterface&),
              (override));
  MOCK_METHOD(base::CallbackListSubscription,
              RegisterStateChange,
              (StateChangeCallback callback),
              (override));
  MOCK_METHOD(base::CallbackListSubscription,
              AddActiveInstanceChangedCallbackAndNotifyImmediately,
              (ActiveInstanceChangedCallback callback),
              (override));
  MOCK_METHOD(GlicInstance*, GetActiveInstance, (), (override));
  MOCK_METHOD(void, SidePanelShown, (BrowserWindowInterface*), (override));
  MOCK_METHOD(Host&, host, (), (override));
  MOCK_METHOD(const InstanceId&, id, (), (const, override));
  MOCK_METHOD(std::optional<std::string>,
              conversation_id,
              (),
              (const, override));
  MOCK_METHOD(base::TimeTicks, GetLastActiveTime, (), (const, override));
  MOCK_METHOD(void, AddGlobalStateObserver, (PanelStateObserver*), (override));
  MOCK_METHOD(void,
              RemoveGlobalStateObserver,
              (PanelStateObserver*),
              (override));
  MOCK_METHOD(glic::GlicInstanceMetrics*, instance_metrics, (), (override));

  base::WeakPtr<GlicWindowControllerInterface> GetWeakPtr() override {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  base::WeakPtrFactory<MockGlicWindowController> weak_ptr_factory_{this};
};
}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_TEST_SUPPORT_MOCK_GLIC_WINDOW_CONTROLLER_H_
