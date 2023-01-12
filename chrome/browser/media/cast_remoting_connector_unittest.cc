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
#include "components/media_router/common/media_route.h"
#include "components/media_router/common/media_source.h"
#include "components/media_router/common/pref_names.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/browser_task_environment.h"
#include "media/mojo/mojom/remoting.mojom.h"
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

RemotingSinkMetadataPtr GetDefaultSinkMetadata() {
  RemotingSinkMetadataPtr metadata = RemotingSinkMetadata::New();
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
  MockRemotingSource() {}
  ~MockRemotingSource() override {}

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

  ~MockMediaRemoter() override {}

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
  MOCK_METHOD0(StartWithPermissionAlreadyGranted, void());
  MOCK_METHOD1(Stop, void(RemotingStopReason));
  MOCK_METHOD1(SendMessageToSink, void(const std::vector<uint8_t>&));
  MOCK_METHOD1(
      EstimateTransmissionCapacity,
      void(media::mojom::Remoter::EstimateTransmissionCapacityCallback));
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
    connector_->CreateBridge(
        std::move(source_pending_remote),
        remoter_pending_remote.InitWithNewPipeAndPassReceiver());
    return remoter_pending_remote;
  }

  static void RunUntilIdle() { base::RunLoop().RunUntilIdle(); }

  void DisableRemoting() {
    connector_->OnStopped(RemotingStopReason::USER_DISABLED);
  }

  void CreateConnector(bool remoting_allowed) {
    connector_.reset();  // Call dtor first if there is one created.
    connector_.reset(new CastRemotingConnector(
        &pref_service_, kRemotingTabId,
        std::make_unique<MediaRemotingDialogCoordinator>()));
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

  EXPECT_CALL(source,
              OnStartFailed(RemotingStartFailReason::REMOTING_NOT_PERMITTED))
      .Times(1);
  remoter->Start();
  RunUntilIdle();

  EXPECT_CALL(source, OnStarted()).Times(1);
  remoter->StartWithPermissionAlreadyGranted();
  RunUntilIdle();
}

TEST_F(CastRemotingConnectorTest, PrefPersistsAcrossReset) {
  CreateConnector(false);
  pref_service_.registry()->RegisterBooleanPref(
      media_router::prefs::kMediaRouterMediaRemotingEnabled, true);
  pref_service_.SetBoolean(
      media_router::prefs::kMediaRouterMediaRemotingEnabled, true);

  // This resets the per-session remoting allowed/disallowed state, but the
  // pref set above should not be affected.
  GetConnector()->ResetRemotingPermission();

  MockRemotingSource source;
  mojo::Remote<media::mojom::Remoter> remoter(CreateRemoter(&source));
  std::unique_ptr<MockMediaRemoter> media_remoter =
      std::make_unique<MockMediaRemoter>(GetConnector());

  EXPECT_CALL(source, OnStarted());
  remoter->Start();
  RunUntilIdle();
}

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
