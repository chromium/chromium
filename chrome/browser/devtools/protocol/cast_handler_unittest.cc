// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/devtools/protocol/cast_handler.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "chrome/browser/media/router/media_router_factory.h"
#include "chrome/browser/media/router/media_sinks_observer.h"
#include "chrome/browser/media/router/test/mock_media_router.h"
#include "chrome/browser/sessions/session_tab_helper.h"
#include "chrome/common/media_router/media_sink.h"
#include "chrome/common/media_router/media_source.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"

using testing::_;
using testing::DoAll;
using testing::Return;
using testing::SaveArg;
using testing::WithArg;

namespace {

constexpr char kRouteId1[] = "route1";
constexpr char kSinkId1[] = "sink1";
constexpr char kSinkId2[] = "sink2";
constexpr char kSinkName1[] = "Sink 1";
constexpr char kSinkName2[] = "Sink 2";

const media_router::MediaSink sink1(kSinkId1,
                                    kSinkName1,
                                    media_router::SinkIconType::CAST);
const media_router::MediaSink sink2(kSinkId2,
                                    kSinkName2,
                                    media_router::SinkIconType::CAST);
const media_router::MediaRoute route1(
    kRouteId1,
    media_router::MediaSource("https://example.com/"),
    kSinkId1,
    "",
    true,
    true);

class MockStartTabMirroringCallback
    : public CastHandler::StartTabMirroringCallback {
 public:
  MOCK_METHOD0(sendSuccess, void());
  MOCK_METHOD1(sendFailure, void(const protocol::DispatchResponse&));
  MOCK_METHOD0(fallThrough, void());
};

}  // namespace

class CastHandlerTest : public ChromeRenderViewHostTestHarness {
 public:
  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    router_ = static_cast<media_router::MockMediaRouter*>(
        media_router::MediaRouterFactory::GetInstance()
            ->SetTestingFactoryAndUse(
                web_contents()->GetBrowserContext(),
                base::BindRepeating(&media_router::MockMediaRouter::Create)));

    // We cannot use std::make_unique<>() here because we're calling a private
    // constructor of CastHandler.
    handler_.reset(new CastHandler(web_contents()));
    EnableHandler();
  }

  void TearDown() override {
    handler_.reset();
    ChromeRenderViewHostTestHarness::TearDown();
  }

 protected:
  void EnableHandler() {
    EXPECT_CALL(*router_, RegisterMediaRoutesObserver(_))
        .WillOnce(SaveArg<0>(&routes_observer_));
    EXPECT_CALL(*router_, RegisterMediaSinksObserver(_))
        .WillRepeatedly(
            WithArg<0>([this](media_router::MediaSinksObserver* observer) {
              if (observer->source())
                sinks_observer_ = observer;
              return true;
            }));
    handler_->Enable(protocol::Maybe<std::string>());
  }

  std::unique_ptr<CastHandler> handler_;
  media_router::MockMediaRouter* router_ = nullptr;
  media_router::MediaRoutesObserver* routes_observer_ = nullptr;
  media_router::MediaSinksObserver* sinks_observer_ = nullptr;
};

TEST_F(CastHandlerTest, SetSinkToUse) {
  sinks_observer_->OnSinksUpdated({sink1, sink2}, {});
  EXPECT_TRUE(handler_->SetSinkToUse(kSinkName1).isSuccess());

  const std::string presentation_url("https://example.com/");
  content::PresentationRequest request(content::GlobalFrameRoutingId(),
                                       {GURL(presentation_url)}, url::Origin());
  // Return sinks when asked for those compatible with |presentation_url|.
  EXPECT_CALL(*router_, RegisterMediaSinksObserver(_))
      .WillOnce(WithArg<0>(
          [presentation_url](media_router::MediaSinksObserver* observer) {
            EXPECT_EQ(presentation_url, observer->source()->id());
            observer->OnSinksUpdated({sink1, sink2}, {});
            return true;
          }));

  EXPECT_CALL(*router_,
              CreateRouteInternal(presentation_url, kSinkId1, _, _, _, _, _));
  media_router::PresentationServiceDelegateImpl::GetOrCreateForWebContents(
      web_contents())
      ->StartPresentation(request, base::DoNothing(), base::DoNothing());
}

TEST_F(CastHandlerTest, StartTabMirroring) {
  sinks_observer_->OnSinksUpdated({sink1, sink2}, {});
  auto callback = std::make_unique<MockStartTabMirroringCallback>();
  auto* callback_ptr = callback.get();

  // Make |router_| return a successful result. |callback| should be notified of
  // the success.
  EXPECT_CALL(*router_, CreateRouteInternal(
                            media_router::MediaSource::ForTab(
                                SessionTabHelper::IdForTab(web_contents()).id())
                                .id(),
                            kSinkId1, _, _, _, _, _))
      .WillOnce(
          WithArg<4>([](media_router::MediaRouteResponseCallback& callback) {
            std::move(callback).Run(
                media_router::mojom::RoutePresentationConnectionPtr(),
                media_router::RouteRequestResult(
                    std::make_unique<media_router::MediaRoute>(route1), "id",
                    "", media_router::RouteRequestResult::OK));
          }));
  EXPECT_CALL(*callback_ptr, sendSuccess());
  handler_->StartTabMirroring(kSinkName1, std::move(callback));
}

TEST_F(CastHandlerTest, StartTabMirroringWithInvalidName) {
  sinks_observer_->OnSinksUpdated({sink1}, {});
  auto callback = std::make_unique<MockStartTabMirroringCallback>();
  auto* callback_ptr = callback.get();

  // Attempting to start casting with a name different from that of the
  // discovered sink should fail.
  EXPECT_CALL(*callback_ptr, sendFailure(_));
  handler_->StartTabMirroring(kSinkName2, std::move(callback));
}

TEST_F(CastHandlerTest, StopCasting) {
  sinks_observer_->OnSinksUpdated({sink1, sink2}, {});
  routes_observer_->OnRoutesUpdated({route1}, {});
  EXPECT_CALL(*router_, TerminateRoute(kRouteId1));
  EXPECT_TRUE(handler_->StopCasting(kSinkName1).isSuccess());
}

TEST_F(CastHandlerTest, StopCastingWithInvalidName) {
  sinks_observer_->OnSinksUpdated({sink1, sink2}, {});
  routes_observer_->OnRoutesUpdated({route1}, {});
  // Attempting to stop casting to a sink without a route should fail.
  EXPECT_EQ(protocol::Response::kError,
            handler_->StopCasting(kSinkName2).status());
}
