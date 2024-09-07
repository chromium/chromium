// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/media_router/browser/presentation/presentation_service_delegate_impl.h"

#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/test/mock_callback.h"
#include "build/build_config.h"
#include "chrome/browser/media/router/presentation/chrome_local_presentation_manager_factory.h"
#include "chrome/browser/media/router/test/provider_test_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "components/media_router/browser/media_router_factory.h"
#include "components/media_router/browser/presentation/local_presentation_manager.h"
#include "components/media_router/browser/presentation/local_presentation_manager_factory.h"
#include "components/media_router/browser/presentation/web_contents_presentation_manager.h"
#include "components/media_router/browser/test/mock_media_router.h"
#include "components/media_router/browser/test/mock_screen_availability_listener.h"
#include "components/media_router/common/media_source.h"
#include "components/media_router/common/pref_names.h"
#include "components/media_router/common/route_request_result.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "content/public/browser/presentation_observer.h"
#include "content/public/browser/presentation_request.h"
#include "content/public/browser/presentation_screen_availability_listener.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/web_contents_tester.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "url/origin.h"

using blink::mojom::PresentationConnection;
using blink::mojom::PresentationConnectionMessage;
using blink::mojom::PresentationConnectionResultPtr;
using blink::mojom::PresentationInfo;
using ::testing::_;
using ::testing::Invoke;
using ::testing::Mock;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::StrictMock;
using ::testing::WithArgs;

namespace {

constexpr char kPresentationUrl1[] = "https://foo.fakeurl.com/";
constexpr char kPresentationUrl2[] = "https://bar.fakeurl.com/";
constexpr char kPresentationUrl3[] = "cast:233637DE";
constexpr char kFrameUrl[] = "http://anotherframeurl.fakeurl.com/";
constexpr char kPresentationId[] = "presentation_id";

// Matches blink::mojom::PresentationInfo.
MATCHER_P(InfoEquals, expected, "") {
  return expected.url == arg.url && expected.id == arg.id;
}

}  // namespace

namespace media_router {

class MockDelegateObserver
    : public content::PresentationServiceDelegate::Observer {
 public:
  MOCK_METHOD0(OnDelegateDestroyed, void());
  MOCK_METHOD1(OnDefaultPresentationStarted, void(const PresentationInfo&));
};

class MockWebContentsPresentationObserver
    : public content::PresentationObserver {
 public:
  explicit MockWebContentsPresentationObserver(
      content::WebContents* web_contents) {
    presentation_manager_ = WebContentsPresentationManager::Get(web_contents);
    presentation_manager_->AddObserver(this);
  }

  ~MockWebContentsPresentationObserver() override {
    if (presentation_manager_)
      presentation_manager_->RemoveObserver(this);
  }

  MOCK_METHOD1(OnPresentationsChanged, void(bool has_presentation));
  MOCK_METHOD1(OnDefaultPresentationChanged,
               void(const content::PresentationRequest* presentation_request));

 private:
  base::WeakPtr<WebContentsPresentationManager> presentation_manager_;
};

class MockCreatePresentationConnectionCallbacks {
 public:
  MOCK_METHOD1(OnCreateConnectionSuccess,
               void(PresentationConnectionResultPtr result));
  MOCK_METHOD1(OnCreateConnectionError,
               void(const blink::mojom::PresentationError& error));
};

class MockLocalPresentationManager : public LocalPresentationManager {
 public:
  void RegisterLocalPresentationController(
      const PresentationInfo& presentation_info,
      const content::GlobalRenderFrameHostId& render_frame_id,
      mojo::PendingRemote<PresentationConnection> controller,
      mojo::PendingReceiver<PresentationConnection> receiver,
      const MediaRoute& route) override {
    RegisterLocalPresentationControllerInternal(
        presentation_info, render_frame_id, controller, receiver, route);
  }

  MOCK_METHOD5(RegisterLocalPresentationControllerInternal,
               void(const PresentationInfo& presentation_info,
                    const content::GlobalRenderFrameHostId& render_frame_id,
                    mojo::PendingRemote<PresentationConnection>& controller,
                    mojo::PendingReceiver<PresentationConnection>& receiver,
                    const MediaRoute& route));
  MOCK_METHOD2(UnregisterLocalPresentationController,
               void(const std::string& presentation_id,
                    const content::GlobalRenderFrameHostId& render_frame_id));
  MOCK_METHOD3(OnLocalPresentationReceiverCreated,
               void(const PresentationInfo& presentation_info,
                    const content::ReceiverConnectionAvailableCallback&
                        receiver_callback,
                    content::WebContents* receiver_web_contents));
  MOCK_METHOD1(OnLocalPresentationReceiverTerminated,
               void(const std::string& presentation_id));
  MOCK_METHOD1(IsLocalPresentation, bool(const std::string& presentation_id));
  MOCK_METHOD1(GetRoute, MediaRoute*(const std::string& presentation_id));
};

std::unique_ptr<KeyedService> BuildMockLocalPresentationManager(
    content::BrowserContext* context) {
  return std::make_unique<NiceMock<MockLocalPresentationManager>>();
}

// TODO(crbug.com/40753746): move p_s_d_impl_unittest.cc to the same directory
// as presentation_service_delegate_impl.* files.
class PresentationServiceDelegateImplTest
    : public ChromeRenderViewHostTestHarness {
 public:
  PresentationServiceDelegateImplTest()
      : router_(nullptr),
        delegate_impl_(nullptr),
        presentation_url1_(kPresentationUrl1),
        presentation_url2_(kPresentationUrl2),
        presentation_urls_({presentation_url1_}),
        frame_url_(kFrameUrl),
        frame_origin_(url::Origin::Create(GURL(frame_url_))),
        source1_(MediaSource::ForPresentationUrl(presentation_url1_)),
        source2_(MediaSource::ForPresentationUrl(presentation_url2_)),
        listener1_(presentation_url1_),
        listener2_(presentation_url2_) {}

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    content::WebContents* wc = GetWebContents();
    router_ = static_cast<MockMediaRouter*>(
        MediaRouterFactory::GetInstance()->SetTestingFactoryAndUse(
            wc->GetBrowserContext(),
            base::BindRepeating(&MockMediaRouter::Create)));
    ASSERT_TRUE(wc);
    PresentationServiceDelegateImpl::CreateForWebContents(wc);
    delegate_impl_ = PresentationServiceDelegateImpl::FromWebContents(wc);
    SetMainFrame();
    presentation_request_ = std::make_unique<content::PresentationRequest>(
        content::GlobalRenderFrameHostId(main_frame_process_id_,
                                         main_frame_routing_id_),
        presentation_urls_, frame_origin_);
    SetMockLocalPresentationManager();
  }

  MOCK_METHOD1(OnDefaultPresentationStarted,
               void(PresentationConnectionResultPtr result));

 protected:
  virtual content::WebContents* GetWebContents() { return web_contents(); }

  MockLocalPresentationManager& GetMockLocalPresentationManager() {
    return *mock_local_manager_;
  }

  void RunDefaultPresentationUrlCallbackTest() {
    auto callback = base::BindRepeating(
        &PresentationServiceDelegateImplTest::OnDefaultPresentationStarted,
        base::Unretained(this));
    std::vector<std::string> urls({kPresentationUrl1});
    delegate_impl_->SetDefaultPresentationUrls(*presentation_request_,
                                               callback);

    ASSERT_TRUE(delegate_impl_->HasDefaultPresentationRequest());
    const auto& request = delegate_impl_->GetDefaultPresentationRequest();

    // Should not trigger callback since route response is error.
    std::unique_ptr<RouteRequestResult> result = RouteRequestResult::FromError(
        "Error", mojom::RouteRequestResultCode::UNKNOWN_ERROR);
    delegate_impl_->OnPresentationResponse(request, /** connection */ nullptr,
                                           *result);
    EXPECT_TRUE(Mock::VerifyAndClearExpectations(this));

    // Should not trigger callback since request doesn't match.
    content::PresentationRequest different_request(
        content::GlobalRenderFrameHostId(100, 200), {presentation_url2_},
        frame_origin_);
    MediaRoute media_route("differentRouteId", source2_, "mediaSinkId", "",
                           true);
    result =
        RouteRequestResult::FromSuccess(media_route, "differentPresentationId");
    delegate_impl_->OnPresentationResponse(different_request,
                                           /** connection */ nullptr, *result);
    EXPECT_TRUE(Mock::VerifyAndClearExpectations(this));

    // Should trigger callback since request matches.
    EXPECT_CALL(*this, OnDefaultPresentationStarted(_)).Times(1);
    MediaRoute media_route2("routeId", source1_, "mediaSinkId", "", true);
    result = RouteRequestResult::FromSuccess(media_route2, "presentationId");
    delegate_impl_->OnPresentationResponse(request, /** connection */ nullptr,
                                           *result);
  }

  void SetMainFrame() {
    content::RenderFrameHost* main_frame =
        GetWebContents()->GetPrimaryMainFrame();
    ASSERT_TRUE(main_frame);
    main_frame_process_id_ = main_frame->GetProcess()->GetID();
    main_frame_routing_id_ = main_frame->GetRoutingID();
  }

  void SetMockLocalPresentationManager() {
    ChromeLocalPresentationManagerFactory::GetInstance()->SetTestingFactory(
        profile(), base::BindRepeating(&BuildMockLocalPresentationManager));
    mock_local_manager_ = static_cast<MockLocalPresentationManager*>(
        LocalPresentationManagerFactory::GetOrCreateForBrowserContext(
            profile()));
  }

  raw_ptr<MockMediaRouter, DanglingUntriaged> router_;
  raw_ptr<PresentationServiceDelegateImpl, DanglingUntriaged> delegate_impl_;
  const GURL presentation_url1_;
  const GURL presentation_url2_;
  std::vector<GURL> presentation_urls_;
  const GURL frame_url_;
  const url::Origin frame_origin_;
  raw_ptr<MockLocalPresentationManager, DanglingUntriaged> mock_local_manager_;

  // |source1_| and |source2_| correspond to |presentation_url1_| and
  // |presentation_url2_|, respectively.
  MediaSource source1_;
  MediaSource source2_;

  // |listener1_| and |listener2_| correspond to |presentation_url1_| and
  // |presentation_url2_|, respectively.
  MockScreenAvailabilityListener listener1_;
  MockScreenAvailabilityListener listener2_;

  // Set in SetMainFrame().
  int main_frame_process_id_ = 0;
  int main_frame_routing_id_ = 0;

  // Set in SetUp().
  std::unique_ptr<content::PresentationRequest> presentation_request_;
};

TEST_F(PresentationServiceDelegateImplTest, AddScreenAvailabilityListener) {
  EXPECT_CALL(*router_, RegisterMediaSinksObserver(_)).WillOnce(Return(true));
  EXPECT_TRUE(delegate_impl_->AddScreenAvailabilityListener(
      main_frame_process_id_, main_frame_routing_id_, &listener1_));
  EXPECT_TRUE(delegate_impl_->HasScreenAvailabilityListenerForTest(
      main_frame_process_id_, main_frame_routing_id_, source1_.id()))
      << "Mapping not found for " << source1_;

  EXPECT_CALL(*router_, UnregisterMediaSinksObserver(_));
  delegate_impl_->RemoveScreenAvailabilityListener(
      main_frame_process_id_, main_frame_routing_id_, &listener1_);
  EXPECT_FALSE(delegate_impl_->HasScreenAvailabilityListenerForTest(
      main_frame_process_id_, main_frame_routing_id_, source1_.id()));
}

TEST_F(PresentationServiceDelegateImplTest, AddMultipleListenersToFrame) {
  ON_CALL(*router_, RegisterMediaSinksObserver(_)).WillByDefault(Return(true));

  EXPECT_CALL(*router_, RegisterMediaSinksObserver(_)).Times(2);
  EXPECT_TRUE(delegate_impl_->AddScreenAvailabilityListener(
      main_frame_process_id_, main_frame_routing_id_, &listener1_));
  EXPECT_TRUE(delegate_impl_->AddScreenAvailabilityListener(
      main_frame_process_id_, main_frame_routing_id_, &listener2_));
  EXPECT_TRUE(delegate_impl_->HasScreenAvailabilityListenerForTest(
      main_frame_process_id_, main_frame_routing_id_, source1_.id()))
      << "Mapping not found for " << source1_;
  EXPECT_TRUE(delegate_impl_->HasScreenAvailabilityListenerForTest(
      main_frame_process_id_, main_frame_routing_id_, source2_.id()))
      << "Mapping not found for " << source2_;

  EXPECT_CALL(*router_, UnregisterMediaSinksObserver(_)).Times(2);
  delegate_impl_->RemoveScreenAvailabilityListener(
      main_frame_process_id_, main_frame_routing_id_, &listener1_);
  delegate_impl_->RemoveScreenAvailabilityListener(
      main_frame_process_id_, main_frame_routing_id_, &listener2_);
  EXPECT_FALSE(delegate_impl_->HasScreenAvailabilityListenerForTest(
      main_frame_process_id_, main_frame_routing_id_, source1_.id()));
  EXPECT_FALSE(delegate_impl_->HasScreenAvailabilityListenerForTest(
      main_frame_process_id_, main_frame_routing_id_, source2_.id()));
}

TEST_F(PresentationServiceDelegateImplTest, AddSameListenerTwice) {
  EXPECT_CALL(*router_, RegisterMediaSinksObserver(_)).WillOnce(Return(true));
  EXPECT_TRUE(delegate_impl_->AddScreenAvailabilityListener(
      main_frame_process_id_, main_frame_routing_id_, &listener1_));
  EXPECT_FALSE(delegate_impl_->AddScreenAvailabilityListener(
      main_frame_process_id_, main_frame_routing_id_, &listener1_));
  EXPECT_TRUE(delegate_impl_->HasScreenAvailabilityListenerForTest(
      main_frame_process_id_, main_frame_routing_id_, source1_.id()));

  EXPECT_CALL(*router_, UnregisterMediaSinksObserver(_)).Times(1);
  delegate_impl_->RemoveScreenAvailabilityListener(
      main_frame_process_id_, main_frame_routing_id_, &listener1_);
  EXPECT_FALSE(delegate_impl_->HasScreenAvailabilityListenerForTest(
      main_frame_process_id_, main_frame_routing_id_, source1_.id()));
}

TEST_F(PresentationServiceDelegateImplTest, AddListenerForInvalidUrl) {
  MockScreenAvailabilityListener listener(GURL("unsupported-url://foo"));
  EXPECT_CALL(listener,
              OnScreenAvailabilityChanged(
                  blink::mojom::ScreenAvailability::SOURCE_NOT_SUPPORTED));
  EXPECT_FALSE(delegate_impl_->AddScreenAvailabilityListener(
      main_frame_process_id_, main_frame_routing_id_, &listener));
  EXPECT_CALL(*router_, RegisterMediaSinksObserver(_)).Times(0);
}

TEST_F(PresentationServiceDelegateImplTest, SetDefaultPresentationUrl) {
  EXPECT_FALSE(delegate_impl_->HasDefaultPresentationRequest());

  content::WebContentsTester::For(GetWebContents())
      ->NavigateAndCommit(frame_url_);

  auto callback = base::BindRepeating(
      &PresentationServiceDelegateImplTest::OnDefaultPresentationStarted,
      base::Unretained(this));
  delegate_impl_->SetDefaultPresentationUrls(*presentation_request_, callback);
  ASSERT_TRUE(delegate_impl_->HasDefaultPresentationRequest());
  const auto& request1 = delegate_impl_->GetDefaultPresentationRequest();
  EXPECT_EQ(presentation_urls_, request1.presentation_urls);

  // Set to a new default presentation URL
  std::vector<GURL> new_urls = {presentation_url2_};
  presentation_request_->presentation_urls = new_urls;
  delegate_impl_->SetDefaultPresentationUrls(*presentation_request_, callback);
  ASSERT_TRUE(delegate_impl_->HasDefaultPresentationRequest());
  const auto& request2 = delegate_impl_->GetDefaultPresentationRequest();
  EXPECT_EQ(new_urls, request2.presentation_urls);

  // Remove default presentation URL.
  presentation_request_->presentation_urls.clear();
  delegate_impl_->SetDefaultPresentationUrls(*presentation_request_, callback);
  EXPECT_FALSE(delegate_impl_->HasDefaultPresentationRequest());
}

TEST_F(PresentationServiceDelegateImplTest, DefaultPresentationUrlCallback) {
  RunDefaultPresentationUrlCallbackTest();
}

TEST_F(PresentationServiceDelegateImplTest, NotifyDefaultPresentationChanged) {
  auto callback = base::BindRepeating(
      &PresentationServiceDelegateImplTest::OnDefaultPresentationStarted,
      base::Unretained(this));

  StrictMock<MockWebContentsPresentationObserver> observer(GetWebContents());

  content::WebContentsTester::For(GetWebContents())
      ->NavigateAndCommit(frame_url_);

  std::vector<GURL> request1_urls = {presentation_url1_};
  content::PresentationRequest observed_request1(
      {main_frame_process_id_, main_frame_routing_id_}, request1_urls,
      frame_origin_);
  EXPECT_CALL(observer, OnDefaultPresentationChanged(_)).Times(1);
  delegate_impl_->SetDefaultPresentationUrls(std::move(observed_request1),
                                             callback);
  EXPECT_TRUE(Mock::VerifyAndClearExpectations(&observer));

  std::vector<GURL> request2_urls = {presentation_url2_};
  content::PresentationRequest observed_request2(
      {main_frame_process_id_, main_frame_routing_id_}, request2_urls,
      frame_origin_);
  EXPECT_CALL(observer, OnDefaultPresentationChanged(_)).Times(1);
  delegate_impl_->SetDefaultPresentationUrls(std::move(observed_request2),
                                             callback);
  EXPECT_TRUE(Mock::VerifyAndClearExpectations(&observer));

  // Remove default presentation URL.
  EXPECT_CALL(observer, OnDefaultPresentationChanged(nullptr)).Times(1);
  content::PresentationRequest empty_request(
      {main_frame_process_id_, main_frame_routing_id_}, std::vector<GURL>(),
      frame_origin_);
  delegate_impl_->SetDefaultPresentationUrls(std::move(empty_request),
                                             callback);
}

TEST_F(PresentationServiceDelegateImplTest, NotifyMediaRoutesChanged) {
  const int render_process_id = 100;
  const int render_frame_id = 200;
  const GURL presentation_url3{kPresentationUrl3};
  MediaSource media_source = MediaSource::ForPresentationUrl(presentation_url3);
  content::PresentationRequest request(
      content::GlobalRenderFrameHostId(render_process_id, render_frame_id),
      {presentation_url3}, frame_origin_);
  MediaRoute media_route("differentRouteId", media_source, "mediaSinkId", "",
                         true);
  std::unique_ptr<RouteRequestResult> result =
      RouteRequestResult::FromSuccess(media_route, kPresentationId);
  StrictMock<MockWebContentsPresentationObserver> observer(GetWebContents());

  EXPECT_CALL(observer, OnPresentationsChanged(true));
  delegate_impl_->OnPresentationResponse(request,
                                         /** connection */ nullptr, *result);

  EXPECT_CALL(observer, OnPresentationsChanged(false));
  delegate_impl_->Terminate(render_process_id, render_frame_id,
                            kPresentationId);
}

TEST_F(PresentationServiceDelegateImplTest, ListenForConnectionStateChange) {
  const MediaRoute::Id route_id("routeId");
  content::WebContentsTester::For(GetWebContents())
      ->NavigateAndCommit(frame_url_);

  // Set up a PresentationConnection so we can listen to it.
  MediaRouteResponseCallback route_response_callback;
  EXPECT_CALL(*router_, JoinRouteInternal(_, _, _, _, _, _))
      .WillOnce(WithArgs<4>(Invoke(
          [&route_response_callback](MediaRouteResponseCallback& callback) {
            route_response_callback = std::move(callback);
          })));

  const std::string kPresentationId("pid");
  presentation_urls_.push_back(GURL(kPresentationUrl3));

  auto& mock_local_manager = GetMockLocalPresentationManager();
  EXPECT_CALL(mock_local_manager, IsLocalPresentation(kPresentationId))
      .WillRepeatedly(Return(false));

  MockCreatePresentationConnectionCallbacks mock_create_connection_callbacks;
  delegate_impl_->ReconnectPresentation(
      *presentation_request_, kPresentationId,
      base::BindOnce(
          &MockCreatePresentationConnectionCallbacks::OnCreateConnectionSuccess,
          base::Unretained(&mock_create_connection_callbacks)),
      base::BindOnce(
          &MockCreatePresentationConnectionCallbacks::OnCreateConnectionError,
          base::Unretained(&mock_create_connection_callbacks)));

  EXPECT_CALL(mock_create_connection_callbacks, OnCreateConnectionSuccess(_))
      .Times(1);
  std::unique_ptr<RouteRequestResult> result = RouteRequestResult::FromSuccess(
      MediaRoute(route_id, source1_, "mediaSinkId", "description", true),
      kPresentationId);
  std::move(route_response_callback).Run(/** connection */ nullptr, *result);

  base::MockCallback<content::PresentationConnectionStateChangedCallback>
      mock_callback;
  auto callback = mock_callback.Get();
  PresentationInfo connection(presentation_url1_, kPresentationId);
  EXPECT_CALL(*router_,
              OnAddPresentationConnectionStateChangedCallbackInvoked(_));
  delegate_impl_->ListenForConnectionStateChange(
      main_frame_process_id_, main_frame_routing_id_, connection, callback);

  EXPECT_CALL(mock_callback, Run(_));
  router_->NotifyPresentationConnectionStateChange(
      route_id, blink::mojom::PresentationConnectionState::TERMINATED);
}

TEST_F(PresentationServiceDelegateImplTest, GetMediaRoutes) {
  EXPECT_TRUE(delegate_impl_->GetMediaRoutes().empty());

  // Start a session.
  content::PresentationRequest request(
      content::GlobalRenderFrameHostId(main_frame_process_id_,
                                       main_frame_routing_id_),
      {presentation_url1_}, frame_origin_);
  MediaRoute media_route("differentRouteId1", source1_, "mediaSinkId", "",
                         true);
  std::unique_ptr<RouteRequestResult> result =
      RouteRequestResult::FromSuccess(media_route, kPresentationId);
  delegate_impl_->OnPresentationResponse(request,
                                         /** connection */ nullptr, *result);
  EXPECT_EQ(delegate_impl_->GetMediaRoutes().size(), 1u);
  EXPECT_EQ(delegate_impl_->GetMediaRoutes()[0], media_route);

  NiceMock<
      base::MockCallback<content::PresentationConnectionStateChangedCallback>>
      mock_callback;
  PresentationInfo connection(presentation_url1_, kPresentationId);
  delegate_impl_->ListenForConnectionStateChange(
      main_frame_process_id_, main_frame_routing_id_, connection,
      mock_callback.Get());

  // Terminate the session.
  router_->NotifyPresentationConnectionStateChange(
      media_route.media_route_id(),
      blink::mojom::PresentationConnectionState::TERMINATED);
  EXPECT_TRUE(delegate_impl_->GetMediaRoutes().empty());
}

TEST_F(PresentationServiceDelegateImplTest, Reset) {
  EXPECT_CALL(*router_, RegisterMediaSinksObserver(_))
      .WillRepeatedly(Return(true));

  EXPECT_TRUE(delegate_impl_->AddScreenAvailabilityListener(
      main_frame_process_id_, main_frame_routing_id_, &listener1_));
  EXPECT_TRUE(delegate_impl_->HasScreenAvailabilityListenerForTest(
      main_frame_process_id_, main_frame_routing_id_, source1_.id()));
  EXPECT_CALL(*router_, UnregisterMediaSinksObserver(_)).Times(1);
  delegate_impl_->Reset(main_frame_process_id_, main_frame_routing_id_);
  EXPECT_FALSE(delegate_impl_->HasScreenAvailabilityListenerForTest(
      main_frame_process_id_, main_frame_routing_id_, source1_.id()));
}

TEST_F(PresentationServiceDelegateImplTest, DelegateObservers) {
  std::unique_ptr<PresentationServiceDelegateImpl> manager(
      new PresentationServiceDelegateImpl(GetWebContents()));

  StrictMock<MockDelegateObserver> delegate_observer1;
  StrictMock<MockDelegateObserver> delegate_observer2;

  manager->AddObserver(123, 234, &delegate_observer1);
  manager->AddObserver(345, 456, &delegate_observer2);

  // Removes |delegate_observer2|.
  manager->RemoveObserver(345, 456);

  EXPECT_CALL(delegate_observer1, OnDelegateDestroyed()).Times(1);
  manager.reset();
}

TEST_F(PresentationServiceDelegateImplTest, SinksObserverCantRegister) {
  EXPECT_CALL(*router_, RegisterMediaSinksObserver(_)).WillOnce(Return(false));
  EXPECT_CALL(listener1_, OnScreenAvailabilityChanged(
                              blink::mojom::ScreenAvailability::DISABLED));
  EXPECT_FALSE(delegate_impl_->AddScreenAvailabilityListener(
      main_frame_process_id_, main_frame_routing_id_, &listener1_));
}

TEST_F(PresentationServiceDelegateImplTest,
       TestCloseConnectionForLocalPresentation) {
  GURL presentation_url = GURL("http://www.example.com/presentation.html");
  PresentationInfo presentation_info(presentation_url, kPresentationId);
  content::GlobalRenderFrameHostId rfh_id(main_frame_process_id_,
                                          main_frame_routing_id_);
  MediaRoute media_route("route_id",
                         MediaSource::ForPresentationUrl(presentation_url),
                         "mediaSinkId", "", true);
  media_route.set_local_presentation(true);

  auto& mock_local_manager = GetMockLocalPresentationManager();
  EXPECT_CALL(mock_local_manager, IsLocalPresentation(kPresentationId))
      .WillRepeatedly(Return(true));

  base::MockCallback<content::PresentationConnectionCallback> success_cb;
  EXPECT_CALL(success_cb, Run(_));

  delegate_impl_->OnStartPresentationSucceeded(
      rfh_id, success_cb.Get(), presentation_info,
      /** connection */ nullptr, media_route);

  EXPECT_CALL(mock_local_manager,
              UnregisterLocalPresentationController(kPresentationId, rfh_id))
      .Times(1);
  EXPECT_CALL(*router_, DetachRoute(_)).Times(0);

  delegate_impl_->CloseConnection(main_frame_process_id_,
                                  main_frame_routing_id_, kPresentationId);
}

TEST_F(PresentationServiceDelegateImplTest,
       TestReconnectPresentationForLocalPresentation) {
  MediaRoute media_route("route_id",
                         MediaSource::ForPresentationUrl(presentation_url1_),
                         "mediaSinkId", "", true);
  media_route.set_local_presentation(true);

  auto& mock_local_manager = GetMockLocalPresentationManager();
  EXPECT_CALL(mock_local_manager, IsLocalPresentation(kPresentationId))
      .WillRepeatedly(Return(true));
  EXPECT_CALL(mock_local_manager, GetRoute(kPresentationId))
      .WillRepeatedly(Return(&media_route));

  base::MockCallback<content::PresentationConnectionCallback> success_cb;
  base::MockCallback<content::PresentationConnectionErrorCallback> error_cb;
  EXPECT_CALL(success_cb, Run(_));
  EXPECT_CALL(mock_local_manager,
              UnregisterLocalPresentationController(
                  kPresentationId,
                  content::GlobalRenderFrameHostId(main_frame_process_id_,
                                                   main_frame_routing_id_)));

  delegate_impl_->ReconnectPresentation(*presentation_request_, kPresentationId,
                                        success_cb.Get(), error_cb.Get());
  delegate_impl_->Reset(main_frame_process_id_, main_frame_routing_id_);
}

TEST_F(PresentationServiceDelegateImplTest, ConnectToLocalPresentation) {
  content::GlobalRenderFrameHostId rfh_id(main_frame_process_id_,
                                          main_frame_routing_id_);
  PresentationInfo presentation_info(presentation_url1_, kPresentationId);

  MediaRoute media_route("route_id",
                         MediaSource::ForPresentationUrl(presentation_info.url),
                         "mediaSinkId", "", true);
  media_route.set_local_presentation(true);

  mojo::Remote<PresentationConnection> receiver_remote;
  MockPresentationConnectionProxy controller_proxy;
  mojo::Receiver<PresentationConnection> controller_receiver(&controller_proxy);
  auto success_cb = [&controller_receiver,
                     &receiver_remote](PresentationConnectionResultPtr result) {
    controller_receiver.Bind(std::move(result->connection_receiver));
    receiver_remote = mojo::Remote<PresentationConnection>(
        std::move(result->connection_remote));
  };

  mojo::Remote<PresentationConnection> connection_remote;
  MockPresentationConnectionProxy receiver_proxy;
  mojo::Receiver<PresentationConnection> receiver_receiver(&receiver_proxy);
  auto& mock_local_manager = GetMockLocalPresentationManager();
  EXPECT_CALL(mock_local_manager,
              RegisterLocalPresentationControllerInternal(
                  InfoEquals(presentation_info), rfh_id, _, _, media_route))
      .WillOnce([&receiver_receiver, &connection_remote](
                    const PresentationInfo&,
                    const content::GlobalRenderFrameHostId&,
                    mojo::PendingRemote<PresentationConnection>& controller,
                    mojo::PendingReceiver<PresentationConnection>& receiver,
                    const MediaRoute&) {
        ASSERT_TRUE(controller && receiver);
        receiver_receiver.Bind(std::move(receiver));
        connection_remote.Bind(std::move(controller));
      });

  delegate_impl_->OnStartPresentationSucceeded(
      rfh_id,
      base::BindOnce(&decltype(success_cb)::operator(),
                     base::Unretained(&success_cb)),
      presentation_info, /** connection */ nullptr, media_route);

  EXPECT_CALL(controller_proxy, OnMessage(_)).WillOnce([](auto message) {
    EXPECT_TRUE(
        message->Equals(*PresentationConnectionMessage::NewMessage("alpha")));
  });
  connection_remote->OnMessage(
      PresentationConnectionMessage::NewMessage("alpha"));
  base::RunLoop().RunUntilIdle();

  EXPECT_CALL(receiver_proxy, OnMessage(_)).WillOnce([](auto message) {
    EXPECT_TRUE(
        message->Equals(*PresentationConnectionMessage::NewMessage("beta")));
  });
  receiver_remote->OnMessage(PresentationConnectionMessage::NewMessage("beta"));
  base::RunLoop().RunUntilIdle();

  EXPECT_CALL(mock_local_manager,
              UnregisterLocalPresentationController(kPresentationId, rfh_id));
  EXPECT_CALL(*router_, DetachRoute(_)).Times(0);
  delegate_impl_->Reset(main_frame_process_id_, main_frame_routing_id_);
}

TEST_F(PresentationServiceDelegateImplTest, ConnectToPresentation) {
  content::GlobalRenderFrameHostId rfh_id(main_frame_process_id_,
                                          main_frame_routing_id_);
  PresentationInfo presentation_info(presentation_url1_, kPresentationId);

  MediaRoute media_route("route_id",
                         MediaSource::ForPresentationUrl(presentation_info.url),
                         "mediaSinkId", "", true);

  mojo::Remote<PresentationConnection> connection_remote;
  NiceMock<MockPresentationConnectionProxy> mock_proxy;
  mojo::Receiver<PresentationConnection> receiver(&mock_proxy);
  auto success_cb =
      [&receiver, &connection_remote](PresentationConnectionResultPtr result) {
        receiver.Bind(std::move(result->connection_receiver));
        connection_remote = mojo::Remote<PresentationConnection>(
            std::move(result->connection_remote));
      };

  PresentationConnectionMessageObserver* proxy_message_observer = nullptr;
  EXPECT_CALL(*router_, RegisterPresentationConnectionMessageObserver(_))
      .WillOnce(::testing::SaveArg<0>(&proxy_message_observer));
  // Note: This specifically tests the messaging case where no mojo pipe is
  // returned to PresentationServiceDelegateImpl and it is not a local
  // presentation.  If a mojo PresentationConnection _were_ returned, the
  // following route message calls would not take place.
  delegate_impl_->OnStartPresentationSucceeded(
      rfh_id,
      base::BindOnce(&decltype(success_cb)::operator(),
                     base::Unretained(&success_cb)),
      presentation_info, /** connection */ nullptr, media_route);

  EXPECT_CALL(*router_,
              SendRouteMessage(media_route.media_route_id(), "alpha"));
  connection_remote->OnMessage(
      PresentationConnectionMessage::NewMessage("alpha"));
  base::RunLoop().RunUntilIdle();

  EXPECT_CALL(mock_proxy, OnMessage(_)).WillOnce([](auto message) {
    EXPECT_TRUE(
        message->Equals(*PresentationConnectionMessage::NewMessage("beta")));
  });
  std::vector<mojom::RouteMessagePtr> messages;
  messages.emplace_back(mojom::RouteMessage::New(
      mojom::RouteMessage::Type::TEXT, "beta", std::nullopt));
  proxy_message_observer->OnMessagesReceived(std::move(messages));
  base::RunLoop().RunUntilIdle();

  EXPECT_CALL(*router_, UnregisterPresentationConnectionMessageObserver(_));
  EXPECT_CALL(*router_, DetachRoute("route_id")).Times(1);
  delegate_impl_->Reset(main_frame_process_id_, main_frame_routing_id_);
}

#if !BUILDFLAG(IS_ANDROID)
TEST_F(PresentationServiceDelegateImplTest, AutoJoinRequest) {
  std::string origin(frame_origin_.Serialize());
  content::WebContentsTester::For(GetWebContents())
      ->NavigateAndCommit(frame_url_);

  MockCreatePresentationConnectionCallbacks mock_create_connection_callbacks;
  const std::string kPresentationId("auto-join");

  auto& mock_local_manager = GetMockLocalPresentationManager();
  EXPECT_CALL(mock_local_manager, IsLocalPresentation(kPresentationId))
      .WillRepeatedly(Return(false));

  // A request to reconnect to a presentation with the special presentation ID
  // should succeed.
  ASSERT_TRUE(IsAutoJoinPresentationId(kPresentationId));
  EXPECT_CALL(*router_, JoinRouteInternal(_, kPresentationId, _, _, _, _))
      .Times(1);
  delegate_impl_->ReconnectPresentation(
      *presentation_request_, kPresentationId,
      base::BindOnce(
          &MockCreatePresentationConnectionCallbacks::OnCreateConnectionSuccess,
          base::Unretained(&mock_create_connection_callbacks)),
      base::BindOnce(
          &MockCreatePresentationConnectionCallbacks::OnCreateConnectionError,
          base::Unretained(&mock_create_connection_callbacks)));
}
#endif  // !BUILDFLAG(IS_ANDROID)

}  // namespace media_router
