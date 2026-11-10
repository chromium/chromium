// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/cast_remoting_connector.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "build/build_config.h"
#include "chrome/browser/media/remoting_bridge.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/media_router/common/media_route.h"
#include "components/media_router/common/media_source.h"
#include "components/media_router/common/pref_names.h"
#include "components/sessions/core/session_id.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_renderer_host.h"
#include "media/mojo/mojom/remoting.mojom.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "services/network/public/cpp/permissions_policy/permissions_policy_declaration.h"
#include "services/network/public/mojom/permissions_policy/permissions_policy_feature.mojom-shared.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using content::BrowserThread;

using media::mojom::RemotingSinkMetadata;
using media::mojom::RemotingSinkMetadataPtr;
using media::mojom::RemotingStartFailReason;
using media::mojom::RemotingStopReason;

using ::testing::_;
using ::testing::AtLeast;

namespace {

constexpr SessionID kRemotingTabId = SessionID::FromSerializedValue(2);

class TestWebContentsDelegate : public content::WebContentsDelegate {
 public:
  bool IsFullscreenForTabOrPending(
      const content::WebContents* web_contents) override {
    return is_fullscreen_;
  }
  void set_is_fullscreen(bool is_fullscreen) { is_fullscreen_ = is_fullscreen; }

 private:
  bool is_fullscreen_ = true;
};

RemotingSinkMetadataPtr GetDefaultSinkMetadata() {
  RemotingSinkMetadataPtr metadata = RemotingSinkMetadata::New();
  metadata->friendly_name = "Living Room TV";
  metadata->features.push_back(
      media::mojom::RemotingSinkFeature::CONTENT_DECRYPTION);
  metadata->video_capabilities.push_back(
      media::mojom::RemotingSinkVideoCapability::CODEC_VP8);
  metadata->audio_capabilities.push_back(
      media::mojom::RemotingSinkAudioCapability::CODEC_AAC);
  return metadata;
}

class MockRemotingSource final : public media::mojom::RemotingSource {
 public:
  MockRemotingSource() = default;
  ~MockRemotingSource() override = default;

  void Bind(mojo::PendingReceiver<media::mojom::RemotingSource> receiver) {
    receiver_.Bind(std::move(receiver));
  }

  MOCK_METHOD0(OnSinkGone, void());
  MOCK_METHOD0(OnStarted, void());
  MOCK_METHOD1(OnStartFailed, void(RemotingStartFailReason));
  MOCK_METHOD1(OnMessageFromSink, void(const std::vector<uint8_t>&));
  MOCK_METHOD1(OnStopped, void(RemotingStopReason));
  MOCK_METHOD1(OnSinkAvailable, void(const RemotingSinkMetadata&));
  void OnSinkAvailable(RemotingSinkMetadataPtr metadata) override {
    OnSinkAvailable(*metadata);
  }

 private:
  mojo::Receiver<media::mojom::RemotingSource> receiver_{this};
};

class MockMediaRemoter final : public media::mojom::Remoter {
 public:
  explicit MockMediaRemoter(CastRemotingConnector* connector) {
    connector->ConnectWithMediaRemoter(receiver_.BindNewPipeAndPassRemote(),
                                       source_.BindNewPipeAndPassReceiver());
  }

  ~MockMediaRemoter() override = default;

  void OnSinkAvailable() {
    EXPECT_TRUE(source_);
    source_->OnSinkAvailable(GetDefaultSinkMetadata());
  }

  void SendMessageToSource(const std::vector<uint8_t>& message) {
    EXPECT_TRUE(source_);
    source_->OnMessageFromSink(message);
  }

  void OnStopped(RemotingStopReason reason) {
    EXPECT_TRUE(source_);
    source_->OnStopped(reason);
  }

  void OnError() {
    EXPECT_TRUE(source_);
    source_->OnStopped(RemotingStopReason::UNEXPECTED_FAILURE);
  }

  // media::mojom::Remoter implementation.
  MOCK_METHOD0(RequestStart, void());
  MOCK_METHOD1(Stop, void(RemotingStopReason));
  MOCK_METHOD1(SendMessageToSink, void(const std::vector<uint8_t>&));
  void Start() override {
    RequestStart();
    if (source_)
      source_->OnStarted();
  }

  // media::mojom::Remoter implementation.
  MOCK_METHOD4(
      StartDataStreams,
      void(mojo::ScopedDataPipeConsumerHandle audio_pipe,
           mojo::ScopedDataPipeConsumerHandle video_pipe,
           mojo::PendingReceiver<media::mojom::RemotingDataStreamSender>
               audio_sender_receiver,
           mojo::PendingReceiver<media::mojom::RemotingDataStreamSender>
               video_sender_receiver));

 private:
  mojo::Receiver<media::mojom::Remoter> receiver_{this};
  mojo::Remote<media::mojom::RemotingSource> source_;
};

}  // namespace

class CastRemotingConnectorTest : public ::testing::Test {
 public:
  CastRemotingConnectorTest() { CreateConnector(true); }

  void TearDown() final {
    // Allow any pending Mojo operations to complete before destruction. For
    // example, when one end of a Mojo message pipe is closed, a task is posted
    // to later destroy objects that were owned by the message pipe.
    RunUntilIdle();
  }

 protected:
  mojo::PendingRemote<media::mojom::Remoter> CreateRemoter(
      MockRemotingSource* source) {
    mojo::PendingRemote<media::mojom::RemotingSource> source_pending_remote;
    source->Bind(source_pending_remote.InitWithNewPipeAndPassReceiver());
    mojo::PendingRemote<media::mojom::Remoter> remoter_pending_remote;
    RemotingBridge::Client* clients[] = {connector_.get()};
    mojo::MakeSelfOwnedReceiver(
        std::make_unique<RemotingBridge>(clients,
                                         std::move(source_pending_remote)),
        remoter_pending_remote.InitWithNewPipeAndPassReceiver());
    return remoter_pending_remote;
  }

  static void RunUntilIdle() { base::RunLoop().RunUntilIdle(); }

  void DisableRemoting() {
    connector_->OnStopped(RemotingStopReason::USER_DISABLED);
  }

  void CreateConnector(bool remoting_allowed) {
    connector_.reset();  // Call dtor first if there is one created.
    connector_.reset(new CastRemotingConnector(&pref_service_, kRemotingTabId));
    connector_->set_remoting_allowed_for_testing(remoting_allowed);
  }

  CastRemotingConnector* GetConnector() const { return connector_.get(); }

  sync_preferences::TestingPrefServiceSyncable pref_service_;

 private:
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<CastRemotingConnector> connector_;
};

TEST_F(CastRemotingConnectorTest, NeverNotifiesThatSinkIsAvailable) {
  MockRemotingSource source;
  mojo::Remote<media::mojom::Remoter> remoter(CreateRemoter(&source));

  EXPECT_CALL(source, OnSinkAvailable(_)).Times(0);
  EXPECT_CALL(source, OnSinkGone()).Times(AtLeast(0));
  RunUntilIdle();
}

TEST_F(CastRemotingConnectorTest, NotifiesWhenSinkIsAvailableAndThenGone) {
  MockRemotingSource source;
  mojo::Remote<media::mojom::Remoter> remoter(CreateRemoter(&source));

  std::unique_ptr<MockMediaRemoter> media_remoter =
      std::make_unique<MockMediaRemoter>(GetConnector());

  EXPECT_CALL(source, OnSinkAvailable(_)).Times(1);
  media_remoter->OnSinkAvailable();
  RunUntilIdle();

  EXPECT_CALL(source, OnSinkGone()).Times(AtLeast(1));
  media_remoter.reset();
  RunUntilIdle();
}

TEST_F(CastRemotingConnectorTest,
       NotifiesMultipleSourcesWhenSinkIsAvailableAndThenGone) {
  MockRemotingSource source1;
  mojo::Remote<media::mojom::Remoter> remoter1(CreateRemoter(&source1));
  MockRemotingSource source2;
  mojo::Remote<media::mojom::Remoter> remoter2(CreateRemoter(&source2));

  std::unique_ptr<MockMediaRemoter> media_remoter =
      std::make_unique<MockMediaRemoter>(GetConnector());

  EXPECT_CALL(source1, OnSinkAvailable(_)).Times(1);
  EXPECT_CALL(source2, OnSinkAvailable(_)).Times(1);
  media_remoter->OnSinkAvailable();
  RunUntilIdle();

  EXPECT_CALL(source1, OnSinkGone()).Times(AtLeast(1));
  EXPECT_CALL(source2, OnSinkGone()).Times(AtLeast(1));
  media_remoter.reset();
  RunUntilIdle();
}

TEST_F(CastRemotingConnectorTest, HandlesTeardownOfRemotingSourceFirst) {
  std::unique_ptr<MockRemotingSource> source(new MockRemotingSource);
  mojo::Remote<media::mojom::Remoter> remoter(CreateRemoter(source.get()));

  std::unique_ptr<MockMediaRemoter> media_remoter =
      std::make_unique<MockMediaRemoter>(GetConnector());

  EXPECT_CALL(*source, OnSinkAvailable(_)).Times(1);
  media_remoter->OnSinkAvailable();
  RunUntilIdle();

  source.reset();
  RunUntilIdle();
}

TEST_F(CastRemotingConnectorTest, HandlesTeardownOfRemoterFirst) {
  MockRemotingSource source;
  mojo::Remote<media::mojom::Remoter> remoter(CreateRemoter(&source));

  std::unique_ptr<MockMediaRemoter> media_remoter =
      std::make_unique<MockMediaRemoter>(GetConnector());

  EXPECT_CALL(source, OnSinkAvailable(_)).Times(1);
  media_remoter->OnSinkAvailable();
  RunUntilIdle();

  remoter.reset();
  RunUntilIdle();
}

TEST_F(CastRemotingConnectorTest, NoConnectedMediaRemoter) {
  MockRemotingSource source;
  mojo::Remote<media::mojom::Remoter> remoter(CreateRemoter(&source));

  EXPECT_CALL(source,
              OnStartFailed(RemotingStartFailReason::INVALID_ANSWER_MESSAGE))
      .Times(1);
  remoter->Start();
  RunUntilIdle();
}

TEST_F(CastRemotingConnectorTest, UserDisableRemoting) {
  MockRemotingSource source1;
  mojo::Remote<media::mojom::Remoter> remoter1(CreateRemoter(&source1));
  MockRemotingSource source2;
  mojo::Remote<media::mojom::Remoter> remoter2(CreateRemoter(&source2));

  std::unique_ptr<MockMediaRemoter> media_remoter =
      std::make_unique<MockMediaRemoter>(GetConnector());

  EXPECT_CALL(source1, OnSinkAvailable(_)).Times(1);
  EXPECT_CALL(source2, OnSinkAvailable(_)).Times(1);
  media_remoter->OnSinkAvailable();
  RunUntilIdle();

  // All sources will get notified that sink is gone when user explicitly
  // disabled media remoting.
  EXPECT_CALL(source1, OnSinkGone()).Times(AtLeast(1));
  EXPECT_CALL(source2, OnSinkGone()).Times(AtLeast(1));
  DisableRemoting();
  RunUntilIdle();
}

TEST_F(CastRemotingConnectorTest, NoPermissionToStart) {
  CreateConnector(false);
  MockRemotingSource source;
  mojo::Remote<media::mojom::Remoter> remoter(CreateRemoter(&source));
  std::unique_ptr<MockMediaRemoter> media_remoter =
      std::make_unique<MockMediaRemoter>(GetConnector());
  EXPECT_CALL(source, OnSinkAvailable(_)).Times(0);
  media_remoter->OnSinkAvailable();
  RunUntilIdle();

  EXPECT_CALL(source,
              OnStartFailed(RemotingStartFailReason::REMOTING_NOT_PERMITTED))
      .Times(1);
  remoter->Start();
  RunUntilIdle();

  // Granting permission for Remote Playback in the browser allows starting and
  // re-notifies existing registered bridges that the sink is available.
  EXPECT_CALL(source, OnSinkAvailable(_)).Times(1);
  GetConnector()->GrantPermissionForRemotePlayback();

  // A new client created while sink metadata is present and permission is
  // already granted immediately receives OnSinkAvailable in OnClientActivated.
  MockRemotingSource source2;
  EXPECT_CALL(source2, OnSinkAvailable(_)).Times(1);
  mojo::Remote<media::mojom::Remoter> remoter2(CreateRemoter(&source2));
  RunUntilIdle();

  EXPECT_CALL(source, OnStarted()).Times(1);
  EXPECT_CALL(source2, OnSinkGone()).Times(1);
  remoter->Start();
  RunUntilIdle();
}

TEST_F(CastRemotingConnectorTest, RemotePlaybackPermissionClearedOnReset) {
  CreateConnector(false);
  pref_service_.registry()->RegisterBooleanPref(
      media_router::prefs::kMediaRouterMediaRemotingEnabled, false);
  pref_service_.SetBoolean(
      media_router::prefs::kMediaRouterMediaRemotingEnabled, false);
  MockRemotingSource source;
  mojo::Remote<media::mojom::Remoter> remoter(CreateRemoter(&source));
  std::unique_ptr<MockMediaRemoter> media_remoter =
      std::make_unique<MockMediaRemoter>(GetConnector());

  GetConnector()->GrantPermissionForRemotePlayback();
  GetConnector()->ResetRemotingPermission();

  EXPECT_CALL(source,
              OnStartFailed(RemotingStartFailReason::REMOTING_NOT_PERMITTED))
      .Times(1);
  remoter->Start();
  RunUntilIdle();
}

TEST_F(CastRemotingConnectorTest, PrefPersistsAcrossReset) {
  CreateConnector(false);
  pref_service_.registry()->RegisterBooleanPref(
      media_router::prefs::kMediaRouterMediaRemotingEnabled, true);

  // Even when the preference is at its default value (true), resetting
  // permission should allow remoting without prompting.
  GetConnector()->ResetRemotingPermission();

  MockRemotingSource source;
  mojo::Remote<media::mojom::Remoter> remoter(CreateRemoter(&source));
  std::unique_ptr<MockMediaRemoter> media_remoter =
      std::make_unique<MockMediaRemoter>(GetConnector());

  EXPECT_CALL(source, OnStarted());
  remoter->Start();
  RunUntilIdle();
}

#if !BUILDFLAG(IS_ANDROID)
TEST_F(CastRemotingConnectorTest, EnableRemotingMidSession) {
  pref_service_.registry()->RegisterBooleanPref(
      media_router::prefs::kMediaRouterMediaRemotingEnabled, false);
  CreateConnector(false);

  MockRemotingSource source;
  mojo::Remote<media::mojom::Remoter> remoter(CreateRemoter(&source));
  std::unique_ptr<MockMediaRemoter> media_remoter =
      std::make_unique<MockMediaRemoter>(GetConnector());

  // While remoting is disabled by the user preference, OnSinkAvailable() should
  // not notify the source.
  EXPECT_CALL(source, OnSinkAvailable(_)).Times(0);
  media_remoter->OnSinkAvailable();
  RunUntilIdle();

  // Re-enabling remoting mid-session should notify the source that the sink is
  // available.
  EXPECT_CALL(source, OnSinkAvailable(_)).Times(1);
  pref_service_.SetBoolean(
      media_router::prefs::kMediaRouterMediaRemotingEnabled, true);
  RunUntilIdle();

  // The source should now be permitted to start remoting.
  EXPECT_CALL(source, OnStarted()).Times(1);
  remoter->Start();
  RunUntilIdle();
}

TEST_F(CastRemotingConnectorTest, DisableAndReEnableRemotingMidSession) {
  pref_service_.registry()->RegisterBooleanPref(
      media_router::prefs::kMediaRouterMediaRemotingEnabled, true);
  CreateConnector(true);

  MockRemotingSource source;
  mojo::Remote<media::mojom::Remoter> remoter(CreateRemoter(&source));
  std::unique_ptr<MockMediaRemoter> media_remoter =
      std::make_unique<MockMediaRemoter>(GetConnector());

  // Initially available.
  EXPECT_CALL(source, OnSinkAvailable(_)).Times(1);
  media_remoter->OnSinkAvailable();
  RunUntilIdle();

  // Disabling the preference mid-session notifies the source that the sink is
  // gone.
  EXPECT_CALL(source, OnSinkGone()).Times(AtLeast(1));
  pref_service_.SetBoolean(
      media_router::prefs::kMediaRouterMediaRemotingEnabled, false);
  RunUntilIdle();

  // Re-enabling the preference mid-session restores sink availability without
  // requiring the mirroring service to re-announce the sink.
  EXPECT_CALL(source, OnSinkAvailable(_)).Times(1);
  pref_service_.SetBoolean(
      media_router::prefs::kMediaRouterMediaRemotingEnabled, true);
  RunUntilIdle();

  // The source can now start remoting.
  EXPECT_CALL(source, OnStarted()).Times(1);
  remoter->Start();
  RunUntilIdle();
}
#endif  // !BUILDFLAG(IS_ANDROID)

namespace {

// The possible ways a remoting session may be terminated in the "full
// run-through" tests.
enum HowItEnds {
  SOURCE_TERMINATES,  // The render process decides to end remoting.
  MOJO_PIPE_CLOSES,   // A Mojo message pipe closes unexpectedly.
  ROUTE_TERMINATES,   // The Media Router UI was used to terminate the route.
  EXTERNAL_FAILURE,   // The sink is cut-off, perhaps due to a network outage.
  USER_DISABLED,      // Media Remoting was disabled by user.
};

}  // namespace

class CastRemotingConnectorFullSessionTest
    : public CastRemotingConnectorTest,
      public ::testing::WithParamInterface<HowItEnds> {
 public:
  HowItEnds how_it_ends() const { return GetParam(); }
};

// Performs a full run-through of starting and stopping remoting, with
// communications between source and sink established at the correct times, and
// tests that end-to-end behavior is correct depending on what caused the
// remoting session to end.
TEST_P(CastRemotingConnectorFullSessionTest, GoesThroughAllTheMotions) {
  std::unique_ptr<MockRemotingSource> source(new MockRemotingSource());
  mojo::Remote<media::mojom::Remoter> remoter(CreateRemoter(source.get()));
  std::unique_ptr<MockRemotingSource> other_source(new MockRemotingSource());
  mojo::Remote<media::mojom::Remoter> other_remoter(
      CreateRemoter(other_source.get()));
  std::unique_ptr<MockMediaRemoter> media_remoter =
      std::make_unique<MockMediaRemoter>(GetConnector());

  // Throughout this test |other_source| should not participate in the
  // remoting session, and so these method calls should never occur:
  EXPECT_CALL(*other_source, OnStarted()).Times(0);
  EXPECT_CALL(*other_source, OnStopped(_)).Times(0);
  EXPECT_CALL(*other_source, OnMessageFromSink(_)).Times(0);

  // Both sinks should be notified when the Cast Provider tells the connector
  // a remoting sink is available.
  EXPECT_CALL(*source, OnSinkAvailable(_)).Times(1);
  EXPECT_CALL(*other_source, OnSinkAvailable(_)).Times(1);
  media_remoter->OnSinkAvailable();
  RunUntilIdle();

  // When |source| starts a remoting session, |other_source| is notified the
  // sink is gone, the Cast Provider is notified that remoting has started,
  // and |source| is notified that its request was successful.
  EXPECT_CALL(*source, OnStarted()).Times(1).RetiresOnSaturation();
  EXPECT_CALL(*other_source, OnSinkGone()).Times(1).RetiresOnSaturation();
  EXPECT_CALL(*media_remoter, RequestStart()).Times(1).RetiresOnSaturation();
  remoter->Start();
  RunUntilIdle();

  // The |source| should now be able to send binary messages to the sink.
  // |other_source| should not!
  const std::vector<uint8_t> message_to_sink = {3, 1, 4, 1, 5, 9};
  EXPECT_CALL(*media_remoter, SendMessageToSink(message_to_sink))
      .Times(1)
      .RetiresOnSaturation();
  remoter->SendMessageToSink(message_to_sink);
  const std::vector<uint8_t> ignored_message_to_sink = {1, 2, 3};
  EXPECT_CALL(*media_remoter, SendMessageToSink(ignored_message_to_sink))
      .Times(0);
  other_remoter->SendMessageToSink(ignored_message_to_sink);
  RunUntilIdle();

  // The sink should also be able to send binary messages to the |source|.
  const std::vector<uint8_t> message_to_source = {2, 7, 1, 8, 2, 8};
  EXPECT_CALL(*source, OnMessageFromSink(message_to_source))
      .Times(1)
      .RetiresOnSaturation();
  media_remoter->SendMessageToSource(message_to_source);
  RunUntilIdle();

  // The |other_source| should not be allowed to start a remoting session.
  EXPECT_CALL(*other_source,
              OnStartFailed(RemotingStartFailReason::CANNOT_START_MULTIPLE))
      .Times(1)
      .RetiresOnSaturation();
  other_remoter->Start();
  RunUntilIdle();

  // What happens from here depends on how this remoting session will end...
  switch (how_it_ends()) {
    case SOURCE_TERMINATES: {
      // When the |source| stops the remoting session, the Cast Provider is
      // notified the session has stopped, and the |source| receives both an
      // OnStopped() and an OnSinkGone() notification.
      const RemotingStopReason reason = RemotingStopReason::LOCAL_PLAYBACK;
      EXPECT_CALL(*source, OnSinkGone()).Times(1).RetiresOnSaturation();
      EXPECT_CALL(*source, OnStopped(reason)).Times(1).RetiresOnSaturation();
      EXPECT_CALL(*media_remoter, Stop(reason)).Times(1).RetiresOnSaturation();
      remoter->Stop(reason);
      RunUntilIdle();

      // Since remoting is stopped, any further messaging in either direction
      // must be dropped.
      const std::vector<uint8_t> dropped_message_to_sink = {1, 6, 1, 8, 0, 3};
      const std::vector<uint8_t> dropped_message_to_source = {6, 2, 8, 3, 1, 8};
      EXPECT_CALL(*source, OnMessageFromSink(_)).Times(0);
      EXPECT_CALL(*media_remoter, SendMessageToSink(_)).Times(0);
      remoter->SendMessageToSink(dropped_message_to_sink);
      media_remoter->SendMessageToSource(dropped_message_to_source);
      RunUntilIdle();

      // When the sink is ready, the Cast Provider sends a notification to the
      // connector. The connector will notify both sources that a sink is once
      // again available.
      EXPECT_CALL(*source, OnSinkAvailable(_)).Times(1);
      EXPECT_CALL(*other_source, OnSinkAvailable(_)).Times(1);
      media_remoter->OnSinkAvailable();
      RunUntilIdle();

      // When the sink is no longer available, the Cast Provider notifies the
      // connector, and both sources are then notified the sink is gone.
      EXPECT_CALL(*source, OnSinkGone()).Times(AtLeast(1));
      EXPECT_CALL(*other_source, OnSinkGone()).Times(AtLeast(1));
      media_remoter.reset();
      RunUntilIdle();

      break;
    }

    case MOJO_PIPE_CLOSES:
      // When the Mojo pipes for |other_source| close, this should not affect
      // the current remoting session.
      EXPECT_CALL(*media_remoter, Stop(_)).Times(0);
      other_source.reset();
      other_remoter.reset();
      RunUntilIdle();

      // Now, when the Mojo pipes for |source| close, the Cast Provider will be
      // notified that the session has stopped.
      EXPECT_CALL(*media_remoter, Stop(_)).Times(1).RetiresOnSaturation();
      source.reset();
      remoter.reset();
      RunUntilIdle();

      break;

    case ROUTE_TERMINATES:
      // When the Media Router terminates the route (e.g., because a user
      // terminated the route from the UI), the source and sink are immediately
      // cut off from one another.
      EXPECT_CALL(*source, OnSinkGone()).Times(AtLeast(1));
      EXPECT_CALL(*other_source, OnSinkGone()).Times(AtLeast(0));
      // Furthermore, the connector and Cast Provider are also cut off from one
      // another and should not be able to exchange messages anymore. Therefore,
      // the connector will never try to notify the sources that the sink is
      // available again.
      EXPECT_CALL(*source, OnSinkAvailable(_)).Times(0);
      EXPECT_CALL(*other_source, OnSinkAvailable(_)).Times(0);
      EXPECT_CALL(*source, OnStopped(RemotingStopReason::SERVICE_GONE))
          .Times(1)
          .RetiresOnSaturation();
      media_remoter.reset();
      RunUntilIdle();

      break;

    case EXTERNAL_FAILURE: {
      // When the Cast Provider is cut-off from the sink, it sends a fail
      // notification to the connector. The connector, in turn, force-stops the
      // remoting session and notifies the |source|.
      EXPECT_CALL(*source, OnSinkGone()).Times(1).RetiresOnSaturation();
      EXPECT_CALL(*source, OnStopped(RemotingStopReason::UNEXPECTED_FAILURE))
          .Times(1)
          .RetiresOnSaturation();
      EXPECT_CALL(*media_remoter, Stop(RemotingStopReason::UNEXPECTED_FAILURE))
          .Times(1)
          .RetiresOnSaturation();
      media_remoter->OnError();
      RunUntilIdle();

      // Since remoting is stopped, any further messaging in either direction
      // must be dropped.
      const std::vector<uint8_t> dropped_message_to_sink = {1, 6, 1, 8, 0, 3};
      const std::vector<uint8_t> dropped_message_to_source = {6, 2, 8, 3, 1, 8};
      EXPECT_CALL(*source, OnMessageFromSink(_)).Times(0);
      EXPECT_CALL(*media_remoter, SendMessageToSink(_)).Times(0);
      remoter->SendMessageToSink(dropped_message_to_sink);
      media_remoter->SendMessageToSource(dropped_message_to_source);
      RunUntilIdle();

      // When the sink is no longer available, the Cast Provider notifies the
      // connector, and both sources are then notified the sink is gone.
      EXPECT_CALL(*source, OnSinkGone()).Times(AtLeast(1));
      EXPECT_CALL(*other_source, OnSinkGone()).Times(AtLeast(1));
      media_remoter.reset();
      RunUntilIdle();

      break;
    }

    case USER_DISABLED: {
      // When user explicitly disabled remoting, the active remoting session
      // will be stopped.
      EXPECT_CALL(*source, OnSinkGone()).Times(AtLeast(1));
      EXPECT_CALL(*other_source, OnSinkGone()).Times(0);
      EXPECT_CALL(*source, OnSinkAvailable(_)).Times(0);
      EXPECT_CALL(*other_source, OnSinkAvailable(_)).Times(0);
      EXPECT_CALL(*source, OnStopped(RemotingStopReason::USER_DISABLED))
          .Times(1);
      EXPECT_CALL(*media_remoter, Stop(RemotingStopReason::USER_DISABLED))
          .Times(1);
      DisableRemoting();

      // All sources will get notified that sink is gone, and no further
      // remoting sessions can be initiated before user re-enables remoting.
      RunUntilIdle();
      EXPECT_CALL(*source, OnSinkGone()).Times(AtLeast(1));
      EXPECT_CALL(*other_source, OnSinkGone()).Times(AtLeast(1));
      EXPECT_CALL(*source, OnSinkAvailable(_)).Times(0);
      EXPECT_CALL(*other_source, OnSinkAvailable(_)).Times(0);
      media_remoter->OnStopped(RemotingStopReason::USER_DISABLED);
      RunUntilIdle();

      break;
    }
  }
}

INSTANTIATE_TEST_SUITE_P(All,
                         CastRemotingConnectorFullSessionTest,
                         ::testing::Values(SOURCE_TERMINATES,
                                           MOJO_PIPE_CLOSES,
                                           ROUTE_TERMINATES,
                                           EXTERNAL_FAILURE,
                                           USER_DISABLED));

class CastRemotingConnectorRenderFrameTest
    : public ChromeRenderViewHostTestHarness {
 public:
  CastRemotingConnectorRenderFrameTest()
      : ChromeRenderViewHostTestHarness(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    web_contents()->SetDelegate(&web_contents_delegate_);
    pref_service_.registry()->RegisterBooleanPref(
        media_router::prefs::kMediaRouterMediaRemotingEnabled, true);
    connector_ = CastRemotingConnector::CreateForTesting(
        &pref_service_, kRemotingTabId, web_contents());
    connector_->set_remoting_allowed_for_testing(true);
  }

  void TearDown() override {
    connector_.reset();
    web_contents()->SetDelegate(nullptr);
    ChromeRenderViewHostTestHarness::TearDown();
  }

  content::RenderFrameHost* AppendSubframeWithFullscreenPolicy(
      const std::string& frame_name) {
    network::ParsedPermissionsPolicy policy = {
        network::ParsedPermissionsPolicyDeclaration(
            network::mojom::PermissionsPolicyFeature::kFullscreen,
            /*allowed_origins=*/{},
            /*self_if_matches=*/std::nullopt,
            /*matches_all_origins=*/true,
            /*matches_opaque_src=*/false)};
    return content::RenderFrameHostTester::For(main_rfh())
        ->AppendChildWithPolicy(frame_name, policy);
  }

  mojo::PendingRemote<media::mojom::Remoter> CreateRemoterForFrame(
      MockRemotingSource* source,
      content::RenderFrameHost* rfh) {
    mojo::PendingRemote<media::mojom::RemotingSource> source_pending_remote;
    source->Bind(source_pending_remote.InitWithNewPipeAndPassReceiver());
    mojo::PendingRemote<media::mojom::Remoter> remoter_pending_remote;
    RemotingBridge::Client* clients[] = {connector_.get()};
    mojo::MakeSelfOwnedReceiver(
        std::make_unique<RemotingBridge>(
            clients, std::move(source_pending_remote), rfh->GetGlobalId()),
        remoter_pending_remote.InitWithNewPipeAndPassReceiver());
    return remoter_pending_remote;
  }

  CastRemotingConnector* GetConnector() const { return connector_.get(); }

  sync_preferences::TestingPrefServiceSyncable pref_service_;
  std::unique_ptr<CastRemotingConnector> connector_;
  TestWebContentsDelegate web_contents_delegate_;
};

TEST_F(CastRemotingConnectorRenderFrameTest,
       MainFrameReceivesFriendlyNameAndCanStart) {
  content::NavigationSimulator::NavigateAndCommitFromDocument(
      GURL("https://example.com"), main_rfh());

  MockRemotingSource source;
  mojo::Remote<media::mojom::Remoter> remoter(
      CreateRemoterForFrame(&source, main_rfh()));
  std::unique_ptr<MockMediaRemoter> media_remoter =
      std::make_unique<MockMediaRemoter>(GetConnector());

  EXPECT_CALL(source,
              OnSinkAvailable(testing::Field(
                  &RemotingSinkMetadata::friendly_name, "Living Room TV")))
      .Times(1);
  media_remoter->OnSinkAvailable();
  task_environment()->RunUntilIdle();

  EXPECT_CALL(source, OnStarted()).Times(1);
  remoter->Start();
  task_environment()->RunUntilIdle();
}

TEST_F(CastRemotingConnectorRenderFrameTest,
       CrossOriginSubframeDoesNotReceiveFriendlyName) {
  content::NavigationSimulator::NavigateAndCommitFromDocument(
      GURL("https://example.com"), main_rfh());
  content::RenderFrameHost* subframe =
      content::RenderFrameHostTester::For(main_rfh())->AppendChild("subframe");
  subframe = content::NavigationSimulator::NavigateAndCommitFromDocument(
      GURL("https://attacker.com"), subframe);

  MockRemotingSource source;
  mojo::Remote<media::mojom::Remoter> remoter(
      CreateRemoterForFrame(&source, subframe));
  std::unique_ptr<MockMediaRemoter> media_remoter =
      std::make_unique<MockMediaRemoter>(GetConnector());

  EXPECT_CALL(source, OnSinkAvailable(testing::Field(
                          &RemotingSinkMetadata::friendly_name, "")))
      .Times(1);
  media_remoter->OnSinkAvailable();
  task_environment()->RunUntilIdle();
}

TEST_F(
    CastRemotingConnectorRenderFrameTest,
    CrossOriginSubframeRejectedDuringTabMirroringAndDoesNotPreemptMainFrame) {
  content::NavigationSimulator::NavigateAndCommitFromDocument(
      GURL("https://example.com"), main_rfh());
  content::RenderFrameHost* subframe =
      content::RenderFrameHostTester::For(main_rfh())->AppendChild("subframe");
  subframe = content::NavigationSimulator::NavigateAndCommitFromDocument(
      GURL("https://attacker.com"), subframe);

  MockRemotingSource main_source;
  mojo::Remote<media::mojom::Remoter> main_remoter(
      CreateRemoterForFrame(&main_source, main_rfh()));

  MockRemotingSource subframe_source;
  mojo::Remote<media::mojom::Remoter> subframe_remoter(
      CreateRemoterForFrame(&subframe_source, subframe));

  std::unique_ptr<MockMediaRemoter> media_remoter =
      std::make_unique<MockMediaRemoter>(GetConnector());

  EXPECT_CALL(main_source, OnSinkAvailable(_)).Times(1);
  EXPECT_CALL(subframe_source, OnSinkAvailable(_)).Times(1);
  media_remoter->OnSinkAvailable();
  task_environment()->RunUntilIdle();

  // Subframe attempt during Tab Mirroring without fullscreen must fail and not
  // disconnect main frame.
  EXPECT_CALL(subframe_source,
              OnStartFailed(RemotingStartFailReason::REMOTING_NOT_PERMITTED))
      .Times(1);
  EXPECT_CALL(main_source, OnSinkGone()).Times(0);
  subframe_remoter->Start();
  task_environment()->RunUntilIdle();

  // Even if the main frame enters fullscreen, the cross-origin subframe must
  // still be rejected.
  GetConnector()->DidAcquireFullscreen(main_rfh());
  EXPECT_CALL(subframe_source,
              OnStartFailed(RemotingStartFailReason::REMOTING_NOT_PERMITTED))
      .Times(1);
  subframe_remoter->Start();
  task_environment()->RunUntilIdle();

  // Main frame can still start successfully.
  EXPECT_CALL(main_source, OnStarted()).Times(1);
  EXPECT_CALL(subframe_source, OnSinkGone()).Times(1);
  main_remoter->Start();
  task_environment()->RunUntilIdle();
}

TEST_F(CastRemotingConnectorRenderFrameTest,
       CrossOriginSubframeCanStartWhenFullscreenDuringTabMirroring) {
  content::NavigationSimulator::NavigateAndCommitFromDocument(
      GURL("https://example.com"), main_rfh());
  content::RenderFrameHost* subframe =
      AppendSubframeWithFullscreenPolicy("subframe");
  subframe = content::NavigationSimulator::NavigateAndCommitFromDocument(
      GURL("https://attacker.com"), subframe);

  MockRemotingSource subframe_source;
  mojo::Remote<media::mojom::Remoter> subframe_remoter(
      CreateRemoterForFrame(&subframe_source, subframe));

  std::unique_ptr<MockMediaRemoter> media_remoter =
      std::make_unique<MockMediaRemoter>(GetConnector());

  EXPECT_CALL(subframe_source, OnSinkAvailable(_)).Times(1);
  media_remoter->OnSinkAvailable();
  task_environment()->RunUntilIdle();

  // If the WebContents is not actually fullscreen, DidAcquireFullscreen is
  // ignored.
  web_contents_delegate_.set_is_fullscreen(false);
  GetConnector()->DidAcquireFullscreen(subframe);
  EXPECT_CALL(subframe_source,
              OnStartFailed(RemotingStartFailReason::REMOTING_NOT_PERMITTED))
      .Times(1);
  subframe_remoter->Start();
  task_environment()->RunUntilIdle();
  web_contents_delegate_.set_is_fullscreen(true);

  // A cross-origin subframe without the fullscreen permissions policy cannot
  // acquire fullscreen permission.
  content::RenderFrameHost* subframe_no_policy =
      content::RenderFrameHostTester::For(main_rfh())
          ->AppendChild("subframe_no_policy");
  subframe_no_policy =
      content::NavigationSimulator::NavigateAndCommitFromDocument(
          GURL("https://attacker2.com"), subframe_no_policy);
  MockRemotingSource no_policy_source;
  mojo::Remote<media::mojom::Remoter> no_policy_remoter(
      CreateRemoterForFrame(&no_policy_source, subframe_no_policy));
  GetConnector()->DidAcquireFullscreen(subframe_no_policy);
  EXPECT_CALL(no_policy_source,
              OnStartFailed(RemotingStartFailReason::REMOTING_NOT_PERMITTED))
      .Times(1);
  no_policy_remoter->Start();
  task_environment()->RunUntilIdle();

  // Acquiring and then exiting tab fullscreen clears fullscreen permission.
  GetConnector()->DidAcquireFullscreen(subframe);
  GetConnector()->DidToggleFullscreenModeForTab(false, false);
  EXPECT_CALL(subframe_source,
              OnStartFailed(RemotingStartFailReason::REMOTING_NOT_PERMITTED))
      .Times(1);
  subframe_remoter->Start();
  task_environment()->RunUntilIdle();

  // Frame deletion also clears fullscreen permission.
  GetConnector()->DidAcquireFullscreen(subframe);
  GetConnector()->RenderFrameDeleted(subframe);
  EXPECT_CALL(subframe_source,
              OnStartFailed(RemotingStartFailReason::REMOTING_NOT_PERMITTED))
      .Times(1);
  subframe_remoter->Start();
  task_environment()->RunUntilIdle();

  // Subframe with fullscreen policy acquires fullscreen while tab is
  // fullscreen.
  GetConnector()->DidAcquireFullscreen(subframe);

  EXPECT_CALL(subframe_source, OnStarted()).Times(1);
  subframe_remoter->Start();
  task_environment()->RunUntilIdle();
}

TEST_F(CastRemotingConnectorRenderFrameTest,
       CrossOriginSubframeAuthorizedForRemotePlaybackCanStart) {
  content::NavigationSimulator::NavigateAndCommitFromDocument(
      GURL("https://example.com"), main_rfh());
  content::RenderFrameHost* subframe =
      content::RenderFrameHostTester::For(main_rfh())->AppendChild("subframe");
  subframe = content::NavigationSimulator::NavigateAndCommitFromDocument(
      GURL("https://attacker.com"), subframe);

  MockRemotingSource main_source;
  mojo::Remote<media::mojom::Remoter> main_remoter(
      CreateRemoterForFrame(&main_source, main_rfh()));

  MockRemotingSource subframe_source;
  mojo::Remote<media::mojom::Remoter> subframe_remoter(
      CreateRemoterForFrame(&subframe_source, subframe));

  std::unique_ptr<MockMediaRemoter> media_remoter =
      std::make_unique<MockMediaRemoter>(GetConnector());

  EXPECT_CALL(main_source, OnSinkAvailable(_)).Times(1);
  EXPECT_CALL(subframe_source, OnSinkAvailable(_)).Times(1);
  media_remoter->OnSinkAvailable();
  task_environment()->RunUntilIdle();

  // Authorizing the main frame for RemotePlayback does not allow a
  // cross-origin subframe to start.
  EXPECT_CALL(main_source, OnSinkAvailable(_)).Times(1);
  EXPECT_CALL(subframe_source, OnSinkAvailable(_)).Times(1);
  GetConnector()->GrantPermissionForRemotePlayback(
      main_rfh()->GetLastCommittedOrigin());
  EXPECT_CALL(subframe_source,
              OnStartFailed(RemotingStartFailReason::REMOTING_NOT_PERMITTED))
      .Times(1);
  subframe_remoter->Start();
  task_environment()->RunUntilIdle();

  // Authorizing the subframe origin allows both the subframe and the main frame
  // to start (the main frame is never locked out).
  EXPECT_CALL(main_source, OnSinkAvailable(_)).Times(1);
  EXPECT_CALL(subframe_source, OnSinkAvailable(_)).Times(1);
  GetConnector()->GrantPermissionForRemotePlayback(
      subframe->GetLastCommittedOrigin());

  EXPECT_CALL(subframe_source, OnStarted()).Times(1);
  EXPECT_CALL(main_source, OnSinkGone()).Times(AtLeast(1));
  subframe_remoter->Start();
  task_environment()->RunUntilIdle();

  EXPECT_CALL(subframe_source, OnStopped(RemotingStopReason::ROUTE_TERMINATED))
      .Times(1);
  EXPECT_CALL(subframe_source, OnSinkGone()).Times(AtLeast(1));
  EXPECT_CALL(main_source, OnSinkAvailable(_)).Times(1);
  EXPECT_CALL(subframe_source, OnSinkAvailable(_)).Times(1);
  media_remoter->OnStopped(RemotingStopReason::ROUTE_TERMINATED);
  media_remoter->OnSinkAvailable();
  task_environment()->RunUntilIdle();

  // Main frame can still start even when subframe origin was authorized.
  EXPECT_CALL(main_source, OnStarted()).Times(1);
  EXPECT_CALL(subframe_source, OnSinkGone()).Times(AtLeast(1));
  main_remoter->Start();
  task_environment()->RunUntilIdle();

  EXPECT_CALL(main_source, OnStopped(RemotingStopReason::ROUTE_TERMINATED))
      .Times(1);
  EXPECT_CALL(main_source, OnSinkGone()).Times(AtLeast(1));
  EXPECT_CALL(main_source, OnSinkAvailable(_)).Times(1);
  EXPECT_CALL(subframe_source, OnSinkAvailable(_)).Times(1);
  media_remoter->OnStopped(RemotingStopReason::ROUTE_TERMINATED);
  media_remoter->OnSinkAvailable();
  task_environment()->RunUntilIdle();

  // Navigating the subframe in-place to a different cross-origin revokes its
  // authorization.
  subframe = content::NavigationSimulator::NavigateAndCommitFromDocument(
      GURL("https://other-attacker.com"), subframe);
  MockRemotingSource navigated_subframe_source;
  mojo::Remote<media::mojom::Remoter> navigated_subframe_remoter(
      CreateRemoterForFrame(&navigated_subframe_source, subframe));
  EXPECT_CALL(navigated_subframe_source,
              OnStartFailed(RemotingStartFailReason::REMOTING_NOT_PERMITTED))
      .Times(1);
  navigated_subframe_remoter->Start();
  task_environment()->RunUntilIdle();
}
