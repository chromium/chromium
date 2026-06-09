// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_TEST_SUPPORT_MOCK_GLIC_INSTANCE_COORDINATOR_H_
#define CHROME_BROWSER_GLIC_TEST_SUPPORT_MOCK_GLIC_INSTANCE_COORDINATOR_H_

#include "chrome/browser/glic/host/context/glic_delegating_sharing_manager.h"
#include "chrome/browser/glic/public/service/glic_instance_coordinator.h"
#include "chrome/browser/glic/widget/glic_view.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace glic {

class MockGlicInstanceCoordinator
    : public testing::NiceMock<GlicInstanceCoordinator> {
 public:
  MockGlicInstanceCoordinator();
  ~MockGlicInstanceCoordinator();

  MOCK_METHOD(bool, IsAnyPanelShowing, (), (const, override));
  MOCK_METHOD(bool,
              IsConversationPresent,
              (const std::string&),
              (const, override));
  MOCK_METHOD(GlicInstanceCoordinator::ActivateTabResult,
              ActivateTabWithConversation,
              (const std::string&),
              (override));
  MOCK_METHOD(GlicInstance*,
              GetInstanceForTab,
              (const tabs::TabInterface*),
              (const, override));
  MOCK_METHOD(GlicInstance*,
              GetInstanceWithGlicWebContents,
              (content::WebContents*),
              (const, override));

  MOCK_METHOD(void,
              Toggle,
              (BrowserWindowInterface*, bool, mojom::InvocationSource),
              (override));
  MOCK_METHOD(void, EnsurePreload, (), (override));
  MOCK_METHOD(base::WeakPtr<GlicInstance>,
              Invoke,
              (GlicInvokeOptions),
              (override));
  MOCK_METHOD(void, Attach, (), ());
  MOCK_METHOD(void, Detach, (), ());
  MOCK_METHOD(void, Shutdown, (), (override));
  MOCK_METHOD(void,
              Resize,
              (const gfx::Size&, base::TimeDelta, base::OnceClosure),
              ());
  MOCK_METHOD(void, EnableDragResize, (bool), ());
  MOCK_METHOD(void, SetDraggableAreas, (const std::vector<gfx::Rect>&), ());
  MOCK_METHOD(void, SetMinimumWidgetSize, (const gfx::Size&), ());
  MOCK_METHOD(void, Close, (const CloseOptions&), (override));
  MOCK_METHOD(void,
              CloseInstanceWithFrame,
              (content::RenderFrameHost * render_frame_host),
              (override));
  MOCK_METHOD(void,
              CloseAndShutdownInstanceWithFrame,
              (content::RenderFrameHost * render_frame_host),
              (override));
  MOCK_METHOD(bool, IsShowing, (), (const));
  MOCK_METHOD(bool, IsDetached, (), (const, override));
  MOCK_METHOD(bool,
              IsPanelShowingForBrowser,
              (const BrowserWindowInterface&),
              (const, override));
  MOCK_METHOD(base::CallbackListSubscription,
              AddGlobalShowHideCallback,
              (base::RepeatingClosure),
              (override));
  MOCK_METHOD(void,
              Reload,
              (content::RenderFrameHost * render_frame_host),
              (override));
  MOCK_METHOD(base::CallbackListSubscription,
              AddActiveInstanceChangedCallbackAndNotifyImmediately,
              (ActiveInstanceChangedCallback callback),
              (override));
  MOCK_METHOD(GlicInstance*, GetActiveInstance, (), (override));
  MOCK_METHOD(GlicSharingManager&,
              active_instance_sharing_manager,
              (),
              (override));

  MOCK_METHOD(void,
              CreateNewConversationForTabs,
              (const std::vector<tabs::TabInterface*>&),
              (override));
  MOCK_METHOD(void,
              ShowInstanceForTabs,
              (const std::vector<tabs::TabInterface*>&, const InstanceId&),
              (override));
  MOCK_METHOD(std::vector<ConversationInfo>,
              GetRecentlyActiveInstances,
              (size_t, base::TimeDelta),
              (override));
  MOCK_METHOD(bool,
              IsTabPinnedToAnyInstance,
              (const tabs::TabHandle&),
              (const, override));
  MOCK_METHOD(void,
              UnpinTabsFromAllInstances,
              (base::span<const tabs::TabHandle>, GlicUnpinTrigger),
              (override));
  MOCK_METHOD(void,
              ArchiveInstanceWithFrame,
              (content::RenderFrameHost*),
              (override));

  MOCK_METHOD(void,
              GetExperimentalTriggeringUpdates,
              (mojo::PendingRemote<mojom::ExperimentalTriggeringUpdatesHandler>,
               base::OnceCallback<void(bool)>),
              (override));

 private:
  GlicDelegatingSharingManager dummy_sharing_manager_;
  base::WeakPtrFactory<MockGlicInstanceCoordinator> weak_ptr_factory_{this};
};
}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_TEST_SUPPORT_MOCK_GLIC_INSTANCE_COORDINATOR_H_
