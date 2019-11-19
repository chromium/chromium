// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/media_router_dialog_controller.h"

#include <memory>
#include <vector>

#include "base/bind.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/media_router/media_route.h"
#include "chrome/common/media_router/media_source.h"
#include "chrome/common/media_router/route_request_result.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::Return;

namespace media_router {

class TestMediaRouterDialogController : public MediaRouterDialogController {
 public:
  explicit TestMediaRouterDialogController(content::WebContents* initiator)
      : MediaRouterDialogController(initiator) {}
  ~TestMediaRouterDialogController() override {}

  bool IsShowingMediaRouterDialog() const override { return has_dialog_; }
  void CreateMediaRouterDialog() override { has_dialog_ = true; }
  void CloseMediaRouterDialog() override { has_dialog_ = false; }

 private:
  bool has_dialog_ = false;
};

class MockWebContentsDelegate : public content::WebContentsDelegate {
 public:
  MockWebContentsDelegate() {}
  ~MockWebContentsDelegate() override {}

  MOCK_METHOD1(ActivateContents, void(content::WebContents* web_contents));
};

class MediaRouterDialogControllerTest : public ChromeRenderViewHostTestHarness {
 public:
  MOCK_METHOD3(RequestSuccess,
               void(const blink::mojom::PresentationInfo&,
                    mojom::RoutePresentationConnectionPtr,
                    const MediaRoute&));
  MOCK_METHOD1(RequestError,
               void(const blink::mojom::PresentationError& error));

 protected:
  MediaRouterDialogControllerTest() {}
  ~MediaRouterDialogControllerTest() override {}

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();

    web_contents_delegate_.reset(new MockWebContentsDelegate());
    web_contents()->SetDelegate(web_contents_delegate_.get());
    dialog_controller_.reset(
        new TestMediaRouterDialogController(web_contents()));
  }

  void TearDown() override {
    dialog_controller_.reset();
    web_contents_delegate_.reset();
    ChromeRenderViewHostTestHarness::TearDown();
  }

  bool ShowMediaRouterDialogForPresentation() {
    return dialog_controller_->ShowMediaRouterDialogForPresentation(
        std::make_unique<StartPresentationContext>(
            content::PresentationRequest(
                {1, 2},
                {GURL("http://example.com"), GURL("http://example2.com")},
                url::Origin::Create(GURL("http://google.com"))),
            base::BindOnce(&MediaRouterDialogControllerTest::RequestSuccess,
                           base::Unretained(this)),
            base::BindOnce(&MediaRouterDialogControllerTest::RequestError,
                           base::Unretained(this))));
  }

  std::unique_ptr<TestMediaRouterDialogController> dialog_controller_;
  std::unique_ptr<MockWebContentsDelegate> web_contents_delegate_;
};

#if defined(OS_ANDROID)
// The non-Android implementation is tested in
// MediaRouterDialogControllerImplTest.
TEST_F(MediaRouterDialogControllerTest, CreateForWebContents) {
  MediaRouterDialogController* dialog_controller =
      MediaRouterDialogController::GetOrCreateForWebContents(web_contents());
  ASSERT_NE(dialog_controller, nullptr);
}
#endif

TEST_F(MediaRouterDialogControllerTest, ShowAndHideDialog) {
  EXPECT_CALL(*web_contents_delegate_, ActivateContents(web_contents()));
  EXPECT_TRUE(dialog_controller_->ShowMediaRouterDialog());
  EXPECT_TRUE(dialog_controller_->IsShowingMediaRouterDialog());

  // If a dialog is already shown, ShowMediaRouterDialog() should return false.
  EXPECT_CALL(*web_contents_delegate_, ActivateContents(web_contents()));
  EXPECT_FALSE(dialog_controller_->ShowMediaRouterDialog());

  dialog_controller_->HideMediaRouterDialog();
  EXPECT_FALSE(dialog_controller_->IsShowingMediaRouterDialog());

  // Once the dialog is hidden, ShowMediaRouterDialog() should return true
  // again.
  EXPECT_CALL(*web_contents_delegate_, ActivateContents(web_contents()));
  EXPECT_TRUE(dialog_controller_->ShowMediaRouterDialog());
}

TEST_F(MediaRouterDialogControllerTest, ShowDialogForPresentation) {
  EXPECT_CALL(*web_contents_delegate_, ActivateContents(web_contents()));
  EXPECT_TRUE(ShowMediaRouterDialogForPresentation());
  EXPECT_TRUE(dialog_controller_->IsShowingMediaRouterDialog());

  // If a dialog is already shown, ShowMediaRouterDialogForPresentation() should
  // return false.
  EXPECT_FALSE(ShowMediaRouterDialogForPresentation());

  // The error callback is invoked automatically if StartPresentationContext is
  // destroyed without a response.
  EXPECT_CALL(*this, RequestError(_)).Times(1);
}

TEST_F(MediaRouterDialogControllerTest, StartPresentationContext) {
  auto context = std::make_unique<StartPresentationContext>(
      content::PresentationRequest(
          {1, 2}, {GURL("http://example.com"), GURL("http://example2.com")},
          url::Origin::Create(GURL("http://google.com"))),
      base::BindOnce(&MediaRouterDialogControllerTest::RequestSuccess,
                     base::Unretained(this)),
      base::BindOnce(&MediaRouterDialogControllerTest::RequestError,
                     base::Unretained(this)));

  MediaRoute route("routeId", MediaSource::ForTab(1), "sinkId", "Description",
                   false, false);
  auto result = RouteRequestResult::FromSuccess(route, "presentationId");

  EXPECT_CALL(*this, RequestSuccess(_, _, _)).Times(1);
  EXPECT_CALL(*this, RequestError(_)).Times(0);
  context->HandleRouteResponse(nullptr, *result);
}

}  // namespace media_router
