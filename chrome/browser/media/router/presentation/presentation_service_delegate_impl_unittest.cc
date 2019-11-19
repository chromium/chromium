// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/presentation/presentation_service_delegate_impl.h"

#include "base/bind.h"
#include "base/test/mock_callback.h"
#include "build/build_config.h"
#include "chrome/browser/media/router/media_router_factory.h"
#include "chrome/browser/media/router/presentation/local_presentation_manager.h"
#include "chrome/browser/media/router/presentation/local_presentation_manager_factory.h"
#include "chrome/browser/media/router/test/mock_media_router.h"
#include "chrome/browser/media/router/test/mock_screen_availability_listener.h"
#include "chrome/browser/media/router/test/test_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/media_router/media_source.h"
#include "chrome/common/media_router/route_request_result.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "components/prefs/scoped_user_pref_update.h"
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

class MockDefaultPresentationRequestObserver
    : public PresentationServiceDelegateImpl::
          DefaultPresentationRequestObserver {
 public:
  MOCK_METHOD1(OnDefaultPresentationChanged,
               void(const content::PresentationRequest&));
  MOCK_METHOD0(OnDefaultPresentationRemoved, void());
};

class MockCreatePresentationConnnectionCallbacks {
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
      const content::GlobalFrameRoutingId& render_frame_id,
      mojo::PendingRemote<PresentationConnection> controller,
      mojo::PendingReceiver<PresentationConnection> receiver,
      const MediaRoute& route) override {
    RegisterLocalPresentationControllerInternal(
        presentation_info, render_frame_id, controller, receiver, route);
  }

  MOCK_METHOD5(RegisterLocalPresentationControllerInternal,
               void(const PresentationInfo& presentation_info,
                    const content::GlobalFrameRoutingId& render_frame_id,
                    mojo::PendingRemote<PresentationConnection>& controller,
                    mojo::PendingReceiver<PresentationConnection>& receiver,
                    const MediaRoute& route));
  MOCK_METHOD2(UnregisterLocalPresentationController,
               void(const std::string& presentation_id,
                    const content::GlobalFrameRoutingId& render_frame_id));
  MOCK_METHOD2(OnLocalPresentationReceiverCreated,
               void(const PresentationInfo& presentation_info,
                    const content::ReceiverConnectionAvailableCallback&
                        receiver_callback));
  MOCK_METHOD1(OnLocalPresentationReceiverTerminated,
               void(const std::string& presentation_id));
  MOCK_METHOD1(IsLocalPresentation, bool(const std::string& presentation_id));
  MOCK_METHOD1(GetRoute, MediaRoute*(const std::string& presentation_id));
};

std::unique_ptr<KeyedService> BuildMockLocalPresentationManager(
    content::BrowserContext* context) {
  return std::make_unique<MockLocalPresentationManager>();
}

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
            web_contents()->GetBrowserContext(),
            base::BindRepeating(&MockMediaRouter::Create)));
    ASSERT_TRUE(wc);
    PresentationServiceDelegateImpl::CreateForWebContents(wc);
    delegate_impl_ = PresentationServiceDelegateImpl::FromWebContents(wc);
    SetMainFrame();
    presentation_request_ = std::make_unique<content::PresentationRequest>(
        content::GlobalFrameRoutingId(main_frame_process_id_,
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

  void RunDefaultPresentationUrlCallbackTest(bool incognito) {
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
        "Error", RouteRequestResult::UNKNOWN_ERROR);
    delegate_impl_->OnRouteResponse(request, /** connection */ nullptr,
                                    *result);
    EXPECT_TRUE(Mock::VerifyAndClearExpectations(this));

    // Should not trigger callback since request doesn't match.
    content::PresentationRequest different_request(
        content::GlobalFrameRoutingId(100, 200), {presentation_url2_},
        frame_origin_);
    MediaRoute media_route("differentRouteId", source2_, "mediaSinkId", "",
                           true, true);
    media_route.set_incognito(incognito);
    result =
        RouteRequestResult::FromSuccess(media_route, "differentPresentationId");
    delegate_impl_->OnRouteResponse(different_request,
                                    /** connection */ nullptr, *result);
    EXPECT_TRUE(Mock::VerifyAndClearExpectations(this));

    // Should trigger callback since request matches.
    EXPECT_CALL(*this, OnDefaultPresentationStarted(_)).Times(1);
    MediaRoute media_route2("routeId", source1_, "mediaSinkId", "", true, true);
    media_route2.set_incognito(incognito);
    result = RouteRequestResult::FromSuccess(media_route2, "presentationId");
    delegate_impl_->OnRouteResponse(request, /** connection */ nullptr,
                                    *result);
  }

  void SetMainFrame() {
    content::RenderFrameHost* main_frame = GetWebContents()->GetMainFrame();
    ASSERT_TRUE(main_frame);
    main_frame_process_id_ = main_frame->GetProcess()->GetID();
    main_frame_routing_id_ = main_frame->GetRoutingID();
  }

  void SetMockLocalPresentationManager() {
    LocalPresentationManagerFactory::GetInstanceForTest()->SetTestingFactory(
        profile(), base::BindRepeating(&BuildMockLocalPresentationManager));
    mock_local_manager_ = static_cast<MockLocalPresentationManager*>(
        LocalPresentationManagerFactory::GetOrCreateForBrowserContext(
            profile()));
  }

  MockMediaRouter* router_;
  PresentationServiceDelegateImpl* delegate_impl_;
  const GURL presentation_url1_;
  const GURL presentation_url2_;
  std::vector<GURL> presentation_urls_;
  const GURL frame_url_;
  const url::Origin frame_origin_;
  MockLocalPresentationManager* mock_local_manager_;

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

class PresentationServiceDelegateImplIncognitoTest
    : public PresentationServiceDelegateImplTest {
 public:
  PresentationServiceDelegateImplIncognitoTest()
      : incognito_web_contents_(nullptr) {}

 protected:
  content::WebContents* GetWebContents() override {
    if (!incognito_web_contents_) {
      Profile* incognito_profile = profile()->GetOffTheRecordProfile();
      incognito_web_contents_ =
          content::WebContentsTester::CreateTestWebContents(incognito_profile,
                                                            nullptr);
    }
    return incognito_web_contents_.get();
  }

  void TearDown() override {
    // We must delete the incognito WC first, as that triggers observers which
    // require RenderViewHost, etc., that in turn are deleted by
    // RenderViewHostTestHarness::TearDown().
    incognito_web_contents_.reset();
    PresentationServiceDelegateImplTest::TearDown();
  }

  std::unique_ptr<content::WebContents> incognito_web_contents_;
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
  RunDefaultPresentationUrlCallbackTest(false);
}

TEST_F(PresentationServiceDelegateImplIncognitoTest,
       DefaultPresentationUrlCallback) {
  RunDefaultPresentationUrlCallbackTest(true);
}

TEST_F(PresentationServiceDelegateImplTest,
       DefaultPresentationRequestObserver) {
  auto callback = base::BindRepeating(
      &PresentationServiceDelegateImplTest::OnDefaultPresentationStarted,
      base::Unretained(this));

  StrictMock<MockDefaultPresentationRequestObserver> observer;
  delegate_impl_->AddDefaultPresentationRequestObserver(&observer);

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
  EXPECT_CALL(observer, OnDefaultPresentationRemoved()).Times(1);
  content::PresentationRequest empty_request(
      {main_frame_process_id_, main_frame_routing_id_}, std::vector<GURL>(),
      frame_origin_);
  delegate_impl_->SetDefaultPresentationUrls(std::move(empty_request),
                                             callback);
}

TEST_F(PresentationServiceDelegateImplTest, ListenForConnnectionStateChange) {
  content::WebContentsTester::For(GetWebContents())
      ->NavigateAndCommit(frame_url_);

  // Set up a PresentationConnection so we can listen to it.
  MediaRouteResponseCallback route_response_callback;
  EXPECT_CALL(*router_, JoinRouteInternal(_, _, _, _, _, _, false))
      .WillOnce(WithArgs<4>(Invoke(
          [&route_response_callback](MediaRouteResponseCallback& callback) {
            route_response_callback = std::move(callback);
          })));

  const std::string kPresentationId("pid");
  presentation_urls_.push_back(GURL(kPresentationUrl3));

  auto& mock_local_manager = GetMockLocalPresentationManager();
  EXPECT_CALL(mock_local_manager, IsLocalPresentation(kPresentationId))
      .WillRepeatedly(Return(false));

  MockCreatePresentationConnnectionCallbacks mock_create_connection_callbacks;
  delegate_impl_->ReconnectPresentation(
      *presentation_request_, kPresentationId,
      base::BindOnce(&MockCreatePresentationConnnectionCallbacks::
                         OnCreateConnectionSuccess,
                     base::Unretained(&mock_create_connection_callbacks)),
      base::BindOnce(
          &MockCreatePresentationConnnectionCallbacks::OnCreateConnectionError,
          base::Unretained(&mock_create_connection_callbacks)));

  EXPECT_CALL(mock_create_connection_callbacks, OnCreateConnectionSuccess(_))
      .Times(1);
  std::unique_ptr<RouteRequestResult> result = RouteRequestResult::FromSuccess(
      MediaRoute("routeId", source1_, "mediaSinkId", "description", true, true),
      kPresentationId);
  std::move(route_response_callback).Run(/** connection */ nullptr, *result);

  base::MockCallback<content::PresentationConnectionStateChangedCallback>
      mock_callback;
  auto callback = mock_callback.Get();
  PresentationInfo connection(presentation_url1_, kPresentationId);
  EXPECT_CALL(*router_,
              OnAddPresentationConnectionStateChangedCallbackInvoked(callback));
  delegate_impl_->ListenForConnectionStateChange(
      main_frame_process_id_, main_frame_routing_id_, connection, callback);
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
  content::GlobalFrameRoutingId rfh_id(main_frame_process_id_,
                                       main_frame_routing_id_);
  MediaRoute media_route("route_id",
                         MediaSource::ForPresentationUrl(presentation_url),
                         "mediaSinkId", "", true, true);
  media_route.set_local_presentation(true);

  auto& mock_local_manager = GetMockLocalPresentationManager();
  EXPECT_CALL(mock_local_manager, IsLocalPresentation(kPresentationId))
      .WillRepeatedly(Return(true));

  base::MockCallback<content::PresentationConnectionCallback> success_cb;
  EXPECT_CALL(success_cb, Run(_));

  delegate_impl_->OnStartPresentationSucceeded(
      rfh_id, success_cb.Get(), presentation_info, /** connection */ nullptr,
      media_route);

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
                         "mediaSinkId", "", true, true);
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
                  content::GlobalFrameRoutingId(main_frame_process_id_,
                                                main_frame_routing_id_)));

  delegate_impl_->ReconnectPresentation(*presentation_request_, kPresentationId,
                                        success_cb.Get(), error_cb.Get());
  delegate_impl_->Reset(main_frame_process_id_, main_frame_routing_id_);
}

TEST_F(PresentationServiceDelegateImplTest, ConnectToLocalPresentation) {
  content::GlobalFrameRoutingId rfh_id(main_frame_process_id_,
                                       main_frame_routing_id_);
  PresentationInfo presentation_info(presentation_url1_, kPresentationId);

  MediaRoute media_route("route_id",
                         MediaSource::ForPresentationUrl(presentation_info.url),
                         "mediaSinkId", "", true, true);
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
                    const content::GlobalFrameRoutingId&,
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
  content::GlobalFrameRoutingId rfh_id(main_frame_process_id_,
                                       main_frame_routing_id_);
  PresentationInfo presentation_info(presentation_url1_, kPresentationId);

  MediaRoute media_route("route_id",
                         MediaSource::ForPresentationUrl(presentation_info.url),
                         "mediaSinkId", "", true, true);

  mojo::Remote<PresentationConnection> connection_remote;
  MockPresentationConnectionProxy mock_proxy;
  mojo::Receiver<PresentationConnection> receiver(&mock_proxy);
  auto success_cb =
      [&receiver, &connection_remote](PresentationConnectionResultPtr result) {
        receiver.Bind(std::move(result->connection_receiver));
        connection_remote = mojo::Remote<PresentationConnection>(
            std::move(result->connection_remote));
      };

  RouteMessageObserver* proxy_message_observer = nullptr;
  EXPECT_CALL(*router_, RegisterRouteMessageObserver(_))
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
      mojom::RouteMessage::Type::TEXT, "beta", base::nullopt));
  proxy_message_observer->OnMessagesReceived(std::move(messages));
  base::RunLoop().RunUntilIdle();

  EXPECT_CALL(*router_, UnregisterRouteMessageObserver(_));
  EXPECT_CALL(*router_, DetachRoute("route_id")).Times(1);
  delegate_impl_->Reset(main_frame_process_id_, main_frame_routing_id_);
}

#if !defined(OS_ANDROID)
TEST_F(PresentationServiceDelegateImplTest, AutoJoinRequest) {
  std::string origin(frame_origin_.Serialize());
  content::WebContentsTester::For(GetWebContents())
      ->NavigateAndCommit(frame_url_);

  MockCreatePresentationConnnectionCallbacks mock_create_connection_callbacks;
  const std::string kPresentationId("auto-join");
  ASSERT_TRUE(IsAutoJoinPresentationId(kPresentationId));

  // Set the user preference for |origin| to prefer tab mirroring.
  {
    ListPrefUpdate update(profile()->GetPrefs(),
                          prefs::kMediaRouterTabMirroringSources);
    update->AppendIfNotPresent(std::make_unique<base::Value>(origin));
  }

  auto& mock_local_manager = GetMockLocalPresentationManager();
  EXPECT_CALL(mock_local_manager, IsLocalPresentation(kPresentationId))
      .WillRepeatedly(Return(false));

  // Auto-join requests should be rejected.
  EXPECT_CALL(mock_create_connection_callbacks, OnCreateConnectionError(_));
  EXPECT_CALL(*router_, JoinRouteInternal(_, kPresentationId, _, _, _, _, _))
      .Times(0);
  delegate_impl_->ReconnectPresentation(
      *presentation_request_, kPresentationId,
      base::BindOnce(&MockCreatePresentationConnnectionCallbacks::
                         OnCreateConnectionSuccess,
                     base::Unretained(&mock_create_connection_callbacks)),
      base::BindOnce(
          &MockCreatePresentationConnnectionCallbacks::OnCreateConnectionError,
          base::Unretained(&mock_create_connection_callbacks)));

  // Remove the user preference for |origin|.
  {
    ListPrefUpdate update(profile()->GetPrefs(),
                          prefs::kMediaRouterTabMirroringSources);
    update->Remove(base::Value(origin), nullptr);
  }

  // Auto-join requests should now go through.
  EXPECT_CALL(*router_, JoinRouteInternal(_, kPresentationId, _, _, _, _, _))
      .Times(1);
  delegate_impl_->ReconnectPresentation(
      *presentation_request_, kPresentationId,
      base::BindOnce(&MockCreatePresentationConnnectionCallbacks::
                         OnCreateConnectionSuccess,
                     base::Unretained(&mock_create_connection_callbacks)),
      base::BindOnce(
          &MockCreatePresentationConnnectionCallbacks::OnCreateConnectionError,
          base::Unretained(&mock_create_connection_callbacks)));
}

TEST_F(PresentationServiceDelegateImplIncognitoTest, AutoJoinRequest) {
  std::string origin(frame_origin_.Serialize());
  content::WebContentsTester::For(GetWebContents())
      ->NavigateAndCommit(frame_url_);

  MockCreatePresentationConnnectionCallbacks mock_create_connection_callbacks;
  const std::string kPresentationId("auto-join");
  ASSERT_TRUE(IsAutoJoinPresentationId(kPresentationId));

  // Set the user preference for |origin| to prefer tab mirroring.
  {
    ListPrefUpdate update(profile()->GetOffTheRecordProfile()->GetPrefs(),
                          prefs::kMediaRouterTabMirroringSources);
    update->AppendIfNotPresent(std::make_unique<base::Value>(origin));
  }

  auto& mock_local_manager = GetMockLocalPresentationManager();
  EXPECT_CALL(mock_local_manager, IsLocalPresentation(kPresentationId))
      .WillRepeatedly(Return(false));

  // Setting the pref in incognito shouldn't set it for the non-incognito
  // profile.
  const base::ListValue* non_incognito_origins =
      profile()->GetPrefs()->GetList(prefs::kMediaRouterTabMirroringSources);
  EXPECT_EQ(non_incognito_origins->Find(base::Value(origin)),
            non_incognito_origins->end());

  // Auto-join requests should be rejected.
  EXPECT_CALL(mock_create_connection_callbacks, OnCreateConnectionError(_));
  EXPECT_CALL(*router_, JoinRouteInternal(_, kPresentationId, _, _, _, _, _))
      .Times(0);
  delegate_impl_->ReconnectPresentation(
      *presentation_request_, kPresentationId,
      base::BindOnce(&MockCreatePresentationConnnectionCallbacks::
                         OnCreateConnectionSuccess,
                     base::Unretained(&mock_create_connection_callbacks)),
      base::BindOnce(
          &MockCreatePresentationConnnectionCallbacks::OnCreateConnectionError,
          base::Unretained(&mock_create_connection_callbacks)));

  // Remove the user preference for |origin| in incognito.
  {
    ListPrefUpdate update(profile()->GetOffTheRecordProfile()->GetPrefs(),
                          prefs::kMediaRouterTabMirroringSources);
    update->Remove(base::Value(origin), nullptr);
  }

  // Auto-join requests should now go through.
  EXPECT_CALL(*router_, JoinRouteInternal(_, kPresentationId, _, _, _, _, _))
      .Times(1);
  delegate_impl_->ReconnectPresentation(
      *presentation_request_, kPresentationId,
      base::BindOnce(&MockCreatePresentationConnnectionCallbacks::
                         OnCreateConnectionSuccess,
                     base::Unretained(&mock_create_connection_callbacks)),
      base::BindOnce(
          &MockCreatePresentationConnnectionCallbacks::OnCreateConnectionError,
          base::Unretained(&mock_create_connection_callbacks)));
}
#endif  // !defined(OS_ANDROID)

}  // namespace media_router
