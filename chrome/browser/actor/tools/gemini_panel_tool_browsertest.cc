// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/tools/gemini_panel_tool.h"

#include <memory>

#include "chrome/browser/actor/actor_keyed_service.h"
#include "chrome/browser/actor/actor_surface.h"
#include "chrome/browser/actor/actor_surface_handle.h"
#include "chrome/browser/actor/actor_surface_registry.h"
#include "chrome/browser/actor/actor_test_util.h"
#include "chrome/browser/actor/tools/gemini_panel_tool_request.h"
#include "chrome/browser/actor/tools/tools_test_util.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/glic/public/glic_passkeys.h"
#include "chrome/browser/glic/public/service/glic_instance_coordinator.h"
#include "chrome/browser/glic/test_support/glic_browser_test.h"
#include "components/actor/public/mojom/actor_types.mojom.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace actor {

class ActorGeminiPanelToolBrowserTest
    : public glic::GlicBrowserTestMixin<ActorToolsTest> {
 public:
  ActorGeminiPanelToolBrowserTest() = default;
  ~ActorGeminiPanelToolBrowserTest() override = default;

  static glic::InvokeWithAutoSubmitPasskey GetPassKey() {
    return glic::InvokeWithAutoSubmitPasskeyProvider::GetPassKey();
  }

  ActorSurfaceHandle active_surface_handle() {
    return ActorSurfaceHandle::From(active_tab()->GetHandle());
  }
};

IN_PROC_BROWSER_TEST_F(ActorGeminiPanelToolBrowserTest,
                       PromptAndCloseGeminiPanel) {
  auto* glic_service = glic::GlicKeyedService::Get(GetProfile());
  ASSERT_TRUE(glic_service);
  ASSERT_FALSE(glic_service->instance_coordinator().IsPanelShowingForBrowser(
      *GetBrowser()));

  {
    ActResultFuture result;
    std::unique_ptr<ToolRequest> request =
        std::make_unique<PromptGeminiPanelToolRequest>(
            GetPassKey(), active_surface_handle(), "Generate an image");
    actor_task().Act(ToRequestList(request), result.GetCallback());
    ExpectOkResult(result);
  }

  EXPECT_TRUE(glic_service->instance_coordinator().IsPanelShowingForBrowser(
      *GetBrowser()));

  {
    ActResultFuture result;
    std::unique_ptr<ToolRequest> request =
        std::make_unique<CloseGeminiPanelToolRequest>(active_surface_handle());
    actor_task().Act(ToRequestList(request), result.GetCallback());
    ExpectOkResult(result);
  }

  EXPECT_FALSE(glic_service->instance_coordinator().IsPanelShowingForBrowser(
      *GetBrowser()));
}

IN_PROC_BROWSER_TEST_F(ActorGeminiPanelToolBrowserTest,
                       CloseGeminiPanel_WhenAlreadyClosed) {
  auto* glic_service = glic::GlicKeyedService::Get(GetProfile());
  ASSERT_TRUE(glic_service);
  ASSERT_FALSE(glic_service->instance_coordinator().IsPanelShowingForBrowser(
      *GetBrowser()));

  ActResultFuture result;
  std::unique_ptr<ToolRequest> request =
      std::make_unique<CloseGeminiPanelToolRequest>(active_surface_handle());
  actor_task().Act(ToRequestList(request), result.GetCallback());
  ExpectOkResult(result);

  EXPECT_FALSE(glic_service->instance_coordinator().IsPanelShowingForBrowser(
      *GetBrowser()));
}

IN_PROC_BROWSER_TEST_F(ActorGeminiPanelToolBrowserTest,
                       PromptGeminiPanel_EmptyPrompt) {
  ActResultFuture result;
  std::unique_ptr<ToolRequest> request =
      std::make_unique<PromptGeminiPanelToolRequest>(
          GetPassKey(), active_surface_handle(), "");
  actor_task().Act(ToRequestList(request), result.GetCallback());
  ExpectErrorResult(result, mojom::ActionResultCode::kArgumentsInvalid);
}

IN_PROC_BROWSER_TEST_F(ActorGeminiPanelToolBrowserTest,
                       PromptGeminiPanel_TabWentAway) {
  ActorSurfaceHandle stale_handle = ActorSurfaceHandle::CreateForTesting();
  ActResultFuture result;
  std::unique_ptr<ToolRequest> request =
      std::make_unique<PromptGeminiPanelToolRequest>(GetPassKey(), stale_handle,
                                                     "Hello");
  actor_task().Act(ToRequestList(request), result.GetCallback());
  ExpectErrorResult(result, mojom::ActionResultCode::kTabWentAway);
}

IN_PROC_BROWSER_TEST_F(ActorGeminiPanelToolBrowserTest,
                       CloseGeminiPanel_TabWentAway) {
  ActorSurfaceHandle stale_handle = ActorSurfaceHandle::CreateForTesting();
  ActResultFuture result;
  std::unique_ptr<ToolRequest> request =
      std::make_unique<CloseGeminiPanelToolRequest>(stale_handle);
  actor_task().Act(ToRequestList(request), result.GetCallback());
  ExpectErrorResult(result, mojom::ActionResultCode::kTabWentAway);
}

IN_PROC_BROWSER_TEST_F(ActorGeminiPanelToolBrowserTest, NotInTab) {
  ActorSurface* headless_surface =
      actor_keyed_service().GetSurfaceRegistry().CreateHeadlessWebContents();
  ASSERT_TRUE(headless_surface);

  {
    ActResultFuture result;
    std::unique_ptr<ToolRequest> request =
        std::make_unique<PromptGeminiPanelToolRequest>(
            GetPassKey(), headless_surface->GetHandle(), "Hello");
    actor_task().Act(ToRequestList(request), result.GetCallback());
    ExpectErrorResult(result, mojom::ActionResultCode::kGeminiPanelNotInTab);
  }

  {
    ActResultFuture result;
    std::unique_ptr<ToolRequest> request =
        std::make_unique<CloseGeminiPanelToolRequest>(
            headless_surface->GetHandle());
    actor_task().Act(ToRequestList(request), result.GetCallback());
    ExpectErrorResult(result, mojom::ActionResultCode::kGeminiPanelNotInTab);
  }
}

}  // namespace actor
