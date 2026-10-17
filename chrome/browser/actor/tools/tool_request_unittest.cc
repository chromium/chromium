// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/tools/tool_request.h"

#include <memory>
#include <optional>
#include <string_view>

#include "base/test/run_until.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "base/unguessable_token.h"
#include "build/buildflag.h"
#include "chrome/browser/actor/actor_surface_handle.h"
#include "chrome/browser/actor/actor_surface_impl.h"
#include "chrome/browser/actor/tools/autofill/attempt_form_filling/attempt_form_filling_tool_request.h"
#include "chrome/browser/actor/tools/click_tool_request.h"
#include "chrome/browser/actor/tools/drag_and_release_tool_request.h"
#include "chrome/browser/actor/tools/file_upload_tool_request.h"
#include "chrome/browser/actor/tools/find_and_highlight_tool_request.h"
#include "chrome/browser/actor/tools/history_tool_request.h"
#include "chrome/browser/actor/tools/media_control_tool_request.h"
#include "chrome/browser/actor/tools/move_mouse_tool_request.h"
#include "chrome/browser/actor/tools/navigate_tool_request.h"
#include "chrome/browser/actor/tools/one_time_token/attempt_otp_filling_tool_request.h"
#include "chrome/browser/actor/tools/page_tool_request.h"
#include "chrome/browser/actor/tools/password_manager/attempt_login/attempt_login_tool_request.h"
#include "chrome/browser/actor/tools/password_manager/change_password/change_password_tool_request.h"
#include "chrome/browser/actor/tools/perform_search_tool_request.h"
#include "chrome/browser/actor/tools/script_tool_request.h"
#include "chrome/browser/actor/tools/scroll_to_tool_request.h"
#include "chrome/browser/actor/tools/scroll_tool_request.h"
#include "chrome/browser/actor/tools/select_tool_request.h"
#include "chrome/browser/actor/tools/site_based_search_tool_request.h"
#include "chrome/browser/actor/tools/tool.h"
#include "chrome/browser/actor/tools/translate_page_tool_request.h"
#include "chrome/browser/actor/tools/type_tool_request.h"
#include "chrome/browser/actor/tools/wait_tool_request.h"
#include "chrome/common/actor.mojom.h"
#include "chrome/common/actor/action_result.h"
#include "chrome/common/buildflags.h"
#include "components/actor/core/shared_types.h"
#include "components/tabs/public/mock_tab_interface.h"
#include "components/tabs/public/tab_interface.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/point.h"
#include "url/gurl.h"

#if !BUILDFLAG(SKIP_ANDROID_UNMIGRATED_ACTOR_FILES)
#include "chrome/browser/actor/tools/tab_management_tool_request.h"
#endif

namespace actor {
namespace {

class TestToolRequest : public ToolRequest {
 public:
  // ToolRequest:
  std::string_view Name() const override { return "TestTool"; }
  void Apply(ToolRequestVisitorFunctor&) const override {}
  CreateToolResult CreateTool(TaskId, ToolDelegate&) const override {
    return {/*tool=*/nullptr, MakeOkResult()};
  }
};

class TestTabToolRequest : public TabToolRequest {
 public:
  using TabToolRequest::TabToolRequest;

  // ToolRequest:
  std::string_view Name() const override { return "TestTabTool"; }
  void Apply(ToolRequestVisitorFunctor&) const override {}
  CreateToolResult CreateTool(TaskId, ToolDelegate&) const override {
    return {/*tool=*/nullptr, MakeOkResult()};
  }
};

class ToolRequestTest : public testing::Test {
 protected:
  base::test::TaskEnvironment task_environment_;
  const ActorSurfaceHandle actor_surface_handle_ =
      ActorSurfaceHandle::CreateForTesting();
};

TEST_F(ToolRequestTest, BaseRequestTargetsNoSurface) {
  TestToolRequest request;
  EXPECT_TRUE(request.GetActorSurfaceHandle().is_null());
  EXPECT_TRUE(request.GetActorSurfaceForValidation().is_null());
  EXPECT_FALSE(request.IsTabScoped());
}

TEST_F(ToolRequestTest, SurfaceHandleStaysAfterSurfaceIsGone) {
  tabs::MockTabInterface tab;
  auto actor_surface = std::make_unique<ActorSurfaceImpl>(actor_surface_handle_,
                                                          tab.GetHandle());

  TestTabToolRequest request(actor_surface_handle_);
  EXPECT_EQ(request.GetActorSurfaceHandle(), actor_surface_handle_);
  EXPECT_EQ(request.GetActorSurfaceForValidation(), actor_surface_handle_);
  EXPECT_TRUE(request.IsTabScoped());

  // The handle stays non-null after the surface goes away, so callers can
  // tell that the target is gone. Wait until the tab no longer resolves to
  // the destroyed surface, i.e. a fresh lookup would return a null handle.
  actor_surface.reset();
  ASSERT_TRUE(base::test::RunUntil(
      [&] { return ActorSurfaceHandle::From(tab.GetHandle()).is_null(); }));
  EXPECT_EQ(request.GetActorSurfaceHandle(), actor_surface_handle_);
  EXPECT_EQ(request.GetActorSurfaceHandle().Get(), nullptr);
}

TEST_F(ToolRequestTest, SurfaceConstructorStoresSurface) {
  tabs::MockTabInterface tab;
  ActorSurfaceImpl actor_surface(actor_surface_handle_, tab.GetHandle());

  TestTabToolRequest request(actor_surface_handle_);
  EXPECT_EQ(request.GetActorSurfaceHandle(), actor_surface_handle_);
  EXPECT_EQ(request.GetActorSurfaceHandle().Get(), &actor_surface);
  EXPECT_TRUE(request.IsTabScoped());
}

TEST_F(ToolRequestTest, SubclassSurfaceConstructorsStoreTarget) {
  tabs::MockTabInterface tab;
  ActorSurfaceImpl actor_surface(actor_surface_handle_, tab.GetHandle());
  const PageTarget target(gfx::Point(1, 2));

  auto expect_targets_surface = [&](const TabToolRequest& request) {
    SCOPED_TRACE(request.Name());
    EXPECT_EQ(request.GetActorSurfaceHandle(), actor_surface_handle_);
  };

  expect_targets_surface(AttemptFormFillingToolRequest(
      actor_surface_handle_, /*requests=*/{}, /*credit_card_opaque_token=*/""));
  expect_targets_surface(ClickToolRequest(actor_surface_handle_, target,
                                          mojom::ClickType::kLeft,
                                          mojom::ClickCount::kSingle));
  expect_targets_surface(
      DragAndReleaseToolRequest(actor_surface_handle_, target, target));
  expect_targets_surface(
      FileUploadToolRequest(actor_surface_handle_, target, /*files=*/{}));
  expect_targets_surface(
      FindAndHighlightToolRequest(actor_surface_handle_, "query"));
  expect_targets_surface(HistoryBackToolRequest(actor_surface_handle_));
  expect_targets_surface(HistoryForwardToolRequest(actor_surface_handle_));
  expect_targets_surface(ReloadPageToolRequest(actor_surface_handle_));
  expect_targets_surface(PlayMediaToolRequest(actor_surface_handle_));
  expect_targets_surface(PauseMediaToolRequest(actor_surface_handle_));
  expect_targets_surface(
      SeekMediaToolRequest(actor_surface_handle_, base::Seconds(1)));
  expect_targets_surface(MoveMouseToolRequest(actor_surface_handle_, target));
  expect_targets_surface(
      NavigateToolRequest(actor_surface_handle_, GURL("https://example.com")));
  expect_targets_surface(AttemptOtpFillingToolRequest(
      actor_surface_handle_, /*trigger_fields=*/{}, /*for_signin=*/false));
  expect_targets_surface(AttemptLoginToolRequest(
      actor_surface_handle_, /*password_button=*/std::nullopt,
      /*sign_in_with_google_button=*/std::nullopt));
  expect_targets_surface(ChangePasswordToolRequest(
      actor_surface_handle_, /*current_password_field=*/std::nullopt,
      /*new_password_field=*/std::nullopt,
      /*confirm_new_password_field=*/std::nullopt,
      /*confirm_button=*/std::nullopt));
  expect_targets_surface(
      PerformSearchToolRequest(actor_surface_handle_, "query"));
  expect_targets_surface(ScriptToolRequest(
      actor_surface_handle_, base::UnguessableToken::Create(), "name", "{}"));
  expect_targets_surface(ScrollToToolRequest(actor_surface_handle_, target));
  expect_targets_surface(ScrollToolRequest(actor_surface_handle_, target,
                                           ScrollToolRequest::Direction::kDown,
                                           /*distance=*/10));
  expect_targets_surface(
      SelectToolRequest(actor_surface_handle_, target, "value"));
  expect_targets_surface(SiteBasedSearchToolRequest(actor_surface_handle_,
                                                    "query", "document_id"));
#if !BUILDFLAG(SKIP_ANDROID_UNMIGRATED_ACTOR_FILES)
  expect_targets_surface(ActivateTabToolRequest(actor_surface_handle_));
  expect_targets_surface(CloseTabToolRequest(actor_surface_handle_));
#endif
  expect_targets_surface(TranslatePageToolRequest(actor_surface_handle_));
  expect_targets_surface(TypeToolRequest(actor_surface_handle_, target, "text",
                                         /*follow_by_enter=*/false,
                                         TypeToolRequest::Mode::kReplace));
}

TEST_F(ToolRequestTest, CopiesKeepTarget) {
  tabs::MockTabInterface tab;
  ActorSurfaceImpl actor_surface(actor_surface_handle_, tab.GetHandle());

  TestTabToolRequest request(actor_surface_handle_);
  TestTabToolRequest copy(request);
  EXPECT_EQ(copy.GetActorSurfaceHandle(), actor_surface_handle_);
}

TEST_F(ToolRequestTest, WaitRequestValidatesObservedSurface) {
  tabs::MockTabInterface tab;
  ActorSurfaceImpl actor_surface(actor_surface_handle_, tab.GetHandle());

  WaitToolRequest request(base::Seconds(0), actor_surface_handle_);
  EXPECT_TRUE(request.GetActorSurfaceHandle().is_null());
  EXPECT_EQ(request.GetActorSurfaceForValidation(), actor_surface_handle_);
  EXPECT_TRUE(request.RequiresUrlCheckInCurrentTab());
}

TEST_F(ToolRequestTest, WaitRequestForMissingSurfaceStillRequiresUrlCheck) {
  // No surface is registered under `actor_surface_handle_`, e.g. the surface
  // is gone or the ID is bad. The request still requires the URL check, so it
  // fails with kTabWentAway rather than silently skipping the observation.
  WaitToolRequest request(base::Seconds(0), actor_surface_handle_);
  EXPECT_EQ(request.GetActorSurfaceForValidation(), actor_surface_handle_);
  EXPECT_EQ(request.GetActorSurfaceForValidation().Get(), nullptr);
  EXPECT_TRUE(request.RequiresUrlCheckInCurrentTab());
}

TEST_F(ToolRequestTest, WaitRequestWithoutSurfaceSkipsUrlCheck) {
  WaitToolRequest request(base::Seconds(0));
  EXPECT_TRUE(request.GetActorSurfaceForValidation().is_null());
  EXPECT_FALSE(request.RequiresUrlCheckInCurrentTab());
}

}  // namespace
}  // namespace actor
