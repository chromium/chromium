// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/devtools/protocol/cast_handler.h"

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/media/router/chrome_media_router_factory.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/media_router/browser/media_sinks_observer.h"
#include "components/media_router/browser/presentation/presentation_service_delegate_impl.h"
#include "components/media_router/browser/test/mock_media_router.h"
#include "components/media_router/common/media_route_provider_helper.h"
#include "components/media_router/common/media_sink.h"
#include "components/media_router/common/media_source.h"
#include "components/media_router/common/test/test_helper.h"
#include "components/sessions/content/session_tab_helper.h"
#include "content/public/browser/presentation_request.h"

using media_router::CreateCastSink;
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

const media_router::MediaSink sink1{CreateCastSink(kSinkId1, kSinkName1)};
const media_router::MediaSink sink2{CreateCastSink(kSinkId2, kSinkName2)};

media_router::MediaRoute Route1() {
  return media_router::MediaRoute(
      kRouteId1, media_router::MediaSource("https://example.com/"), kSinkId1,
      "", true);
}

class MockStartDesktopMirroringCallback
    : public CastHandler::StartDesktopMirroringCallback {
 public:
  MOCK_METHOD(void, sendSuccess, ());
  MOCK_METHOD(void, sendFailure, (const protocol::DispatchResponse&));
  MOCK_METHOD(void, fallThrough, ());
};

class MockStartTabMirroringCallback
    : public CastHandler::StartTabMirroringCallback {
 public:
  MOCK_METHOD(void, sendSuccess, ());
  MOCK_METHOD(void, sendFailure, (const protocol::DispatchResponse&));
  MOCK_METHOD(void, fallThrough, ());
};

}  // namespace

class CastHandlerTest : public ChromeRenderViewHostTestHarness {
 public:
  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    router_ = static_cast<media_router::MockMediaRouter*>(
        media_router::ChromeMediaRouterFactory::GetInstance()
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
    EXPECT_CALL(*router_, RegisterMediaSinksObserver(_))
        .WillRepeatedly(
            WithArg<0>([this](media_router::MediaSinksObserver* observer) {
              if (observer->source()) {
                if (observer->source()->IsDesktopMirroringSource()) {
                  desktop_sinks_observer_ = observer;
                } else {
                  sinks_observer_ = observer;
                }
              }
              return true;
            }));
    handler_->Enable(protocol::Maybe<std::string>());
  }

  std::unique_ptr<CastHandler> handler_;
  raw_ptr<media_router::MockMediaRouter, DanglingUntriaged> router_ = nullptr;
  raw_ptr<media_router::MediaSinksObserver, DanglingUntriaged>
      desktop_sinks_observer_ = nullptr;
  raw_ptr<media_router::MediaSinksObserver, DanglingUntriaged> sinks_observer_ =
      nullptr;
};

TEST_F(CastHandlerTest, SetSinkToUse) {
  sinks_observer_->OnSinksUpdated({sink1, sink2}, {});
  EXPECT_TRUE(handler_->SetSinkToUse(kSinkName1).IsSuccess());

  const std::string presentation_url("https://example.com/");
  content::PresentationRequest request(content::GlobalRenderFrameHostId(),
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
              CreateRouteInternal(presentation_url, kSinkId1, _, _, _, _));
  media_router::PresentationServiceDelegateImpl::GetOrCreateForWebContents(
      web_contents())
      ->StartPresentation(request, base::DoNothing(), base::DoNothing());
}

TEST_F(CastHandlerTest, StartDesktopMirroring) {
  desktop_sinks_observer_->OnSinksUpdated({sink1, sink2}, {});
  auto callback = std::make_unique<MockStartDesktopMirroringCallback>();
  auto* callback_ptr = callback.get();

  // Make |router_| return a successful result. |callback| should be notified of
  // the success.
  EXPECT_CALL(
      *router_,
      CreateRouteInternal(media_router::MediaSource::ForUnchosenDesktop().id(),
                          kSinkId1, _, _, _, _))
      .WillOnce(
          WithArg<4>([](media_router::MediaRouteResponseCallback& callback) {
            std::move(callback).Run(
                media_router::mojom::RoutePresentationConnectionPtr(),
                media_router::RouteRequestResult(
                    std::make_unique<media_router::MediaRoute>(Route1()), "id",
                    "", media_router::mojom::RouteRequestResultCode::OK));
          }));
  EXPECT_CALL(*callback_ptr, sendSuccess());
  handler_->StartDesktopMirroring(kSinkName1, std::move(callback));
}

TEST_F(CastHandlerTest, StartDesktopMirroringWithInvalidName) {
  sinks_observer_->OnSinksUpdated({sink1}, {});
  auto callback = std::make_unique<MockStartDesktopMirroringCallback>();

  // Attempting to start casting with a name different from that of the
  // discovered sink should fail.
  EXPECT_CALL(*callback.get(), sendFailure(_));
  handler_->StartDesktopMirroring(kSinkName2, std::move(callback));
}

TEST_F(CastHandlerTest, StartTabMirroring) {
  sinks_observer_->OnSinksUpdated({sink1, sink2}, {});
  auto callback = std::make_unique<MockStartTabMirroringCallback>();
  auto* callback_ptr = callback.get();

  // Make |router_| return a successful result. |callback| should be notified of
  // the success.
  EXPECT_CALL(*router_,
              CreateRouteInternal(
                  media_router::MediaSource::ForTab(
                      sessions::SessionTabHelper::IdForTab(web_contents()).id())
                      .id(),
                  kSinkId1, _, _, _, _))
      .WillOnce(
          WithArg<4>([](media_router::MediaRouteResponseCallback& callback) {
            std::move(callback).Run(
                media_router::mojom::RoutePresentationConnectionPtr(),
                media_router::RouteRequestResult(
                    std::make_unique<media_router::MediaRoute>(Route1()), "id",
                    "", media_router::mojom::RouteRequestResultCode::OK));
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
  router_->routes_observers().begin()->OnRoutesUpdated({Route1()});
  EXPECT_CALL(*router_, TerminateRoute(kRouteId1));
  EXPECT_TRUE(handler_->StopCasting(kSinkName1).IsSuccess());
}

TEST_F(CastHandlerTest, StopCastingWithInvalidName) {
  sinks_observer_->OnSinksUpdated({sink1, sink2}, {});
  router_->routes_observers().begin()->OnRoutesUpdated({Route1()});
  // Attempting to stop casting to a sink without a route should fail.
  EXPECT_TRUE(handler_->StopCasting(kSinkName2).IsError());
}
