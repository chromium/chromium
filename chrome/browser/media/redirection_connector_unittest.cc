// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/redirection_connector.h"

#include <initializer_list>
#include <memory>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/test/run_until.h"
#include "chrome/browser/media/cast_remoting_connector.h"
#include "chrome/browser/media/remoting_bridge.h"
#include "components/sessions/core/session_id.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/test/browser_task_environment.h"
#include "media/mojo/mojom/remoting.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using media::mojom::RemotingSinkMetadata;
using media::mojom::RemotingSinkMetadataPtr;
using media::mojom::RemotingStartFailReason;
using media::mojom::RemotingStopReason;

using ::testing::_;
using ::testing::AtLeast;

namespace {

constexpr SessionID kRemotingTabId = SessionID::FromSerializedValue(2);

// A Remoter standing in for the one the utility process would provide.
class FakeRemoter final : public media::mojom::Remoter {
 public:
  FakeRemoter(mojo::PendingReceiver<media::mojom::Remoter> receiver,
              mojo::PendingRemote<media::mojom::RemotingSource> source)
      : receiver_(this, std::move(receiver)), source_(std::move(source)) {}
  ~FakeRemoter() override = default;

  // Calls the session would make once it is serving media.
  void OnSinkAvailable() {
    source_->OnSinkAvailable(RemotingSinkMetadata::New());
  }
  void OnSinkGone() { source_->OnSinkGone(); }
  void OnStopped(RemotingStopReason reason) { source_->OnStopped(reason); }
  void OnMessageFromSink(const std::vector<uint8_t>& message) {
    source_->OnMessageFromSink(message);
  }

  // What the browser asked of this session.
  int start_count() const { return start_count_; }
  const std::vector<RemotingStopReason>& stop_reasons() const {
    return stop_reasons_;
  }
  const std::vector<std::vector<uint8_t>>& messages_to_sink() const {
    return messages_to_sink_;
  }

  // Settles messages in flight on this end of each pipe. See FlushPipes().
  void FlushRemoterPipe() { receiver_.FlushForTesting(); }
  void FlushSourcePipe() { source_.FlushForTesting(); }

 private:
  // media::mojom::Remoter implementation.
  void Start() override {
    ++start_count_;
    source_->OnStarted();
  }
  void StartWithPermissionAlreadyGranted() override { Start(); }
  void StartDataStreams(
      mojo::ScopedDataPipeConsumerHandle audio_pipe,
      mojo::ScopedDataPipeConsumerHandle video_pipe,
      mojo::PendingReceiver<media::mojom::RemotingDataStreamSender>
          audio_sender,
      mojo::PendingReceiver<media::mojom::RemotingDataStreamSender>
          video_sender) override {}
  void Stop(RemotingStopReason reason) override {
    stop_reasons_.push_back(reason);
  }
  void SendMessageToSink(const std::vector<uint8_t>& message) override {
    messages_to_sink_.push_back(message);
  }
  void EstimateTransmissionCapacity(
      EstimateTransmissionCapacityCallback callback) override {
    std::move(callback).Run(0);
  }

  int start_count_ = 0;
  std::vector<RemotingStopReason> stop_reasons_;
  std::vector<std::vector<uint8_t>> messages_to_sink_;
  mojo::Receiver<media::mojom::Remoter> receiver_;
  mojo::Remote<media::mojom::RemotingSource> source_;
};

// Stands in for the utility process, vending FakeRemoters instead of launching
// one. Supplies the same callback RedirectionServiceHost would.
class FakeRedirectionSessionFactory {
 public:
  FakeRedirectionSessionFactory() = default;
  ~FakeRedirectionSessionFactory() = default;

  RedirectionConnector::CreateRedirectionSessionCallback GetCallback() {
    return base::BindRepeating(&FakeRedirectionSessionFactory::CreateSession,
                               base::Unretained(this));
  }

  size_t remoter_count() const { return remoters_.size(); }
  FakeRemoter* remoter_at(size_t index) { return remoters_[index].get(); }

  // Closes one session's pipes, as the utility process does when it can no
  // longer serve that source. Indices of the other sessions are unaffected.
  void DropSession(size_t index) {
    session_hosts_[index].reset();
    remoters_[index].reset();
  }

 private:
  void CreateSession(
      mojo::PendingReceiver<redirection::mojom::RedirectionSessionHost>
          session_host,
      mojo::PendingReceiver<media::mojom::Remoter> remoter,
      mojo::PendingRemote<media::mojom::RemotingSource> source) {
    session_hosts_.push_back(std::move(session_host));
    remoters_.push_back(
        std::make_unique<FakeRemoter>(std::move(remoter), std::move(source)));
  }

  std::vector<mojo::PendingReceiver<redirection::mojom::RedirectionSessionHost>>
      session_hosts_;
  std::vector<std::unique_ptr<FakeRemoter>> remoters_;
};

class MockRemotingSource : public media::mojom::RemotingSource {
 public:
  // Creates the RemotingBridge serving this source, as the render process
  // would. The bridge is owned by its message pipe, so it lives until
  // |remoter_| is dropped.
  MockRemotingSource(CastRemotingConnector* cast_connector,
                     RedirectionConnector* redirection_connector) {
    RemotingBridge::Client* clients[] = {cast_connector, redirection_connector};
    mojo::MakeSelfOwnedReceiver(
        std::make_unique<RemotingBridge>(clients,
                                         receiver_.BindNewPipeAndPassRemote()),
        remoter_.BindNewPipeAndPassReceiver());
  }
  ~MockRemotingSource() override = default;

  // The Remoter the render process would hold.
  mojo::Remote<media::mojom::Remoter>& remoter() { return remoter_; }

  // Closing either end is how a render process going away reaches the browser.
  void CloseRemoterPipe() { remoter_.reset(); }
  void CloseSourcePipe() { receiver_.reset(); }

  // Settles messages in flight on this end of each pipe. See FlushPipes().
  void FlushRemoterPipe() {
    if (remoter_.is_bound()) {
      remoter_.FlushForTesting();
    }
  }
  void FlushSourcePipe() {
    if (receiver_.is_bound()) {
      receiver_.FlushForTesting();
    }
  }

  MOCK_METHOD0(OnSinkGone, void());
  MOCK_METHOD0(OnStarted, void());
  MOCK_METHOD1(OnStartFailed, void(RemotingStartFailReason));
  MOCK_METHOD1(OnMessageFromSink, void(const std::vector<uint8_t>&));
  MOCK_METHOD1(OnStopped, void(RemotingStopReason));
  MOCK_METHOD0(OnSinkAvailable, void());
  void OnSinkAvailable(RemotingSinkMetadataPtr metadata) override {
    last_sink_metadata_ = std::move(metadata);
    OnSinkAvailable();
  }

  const RemotingSinkMetadataPtr& last_sink_metadata() const {
    return last_sink_metadata_;
  }

 private:
  RemotingSinkMetadataPtr last_sink_metadata_;
  mojo::Remote<media::mojom::Remoter> remoter_;
  mojo::Receiver<media::mojom::RemotingSource> receiver_{this};
};

}  // namespace

class RedirectionConnectorTest : public ::testing::Test {
 public:
  RedirectionConnectorTest()
      : cast_connector_(&pref_service_,
                        kRemotingTabId,
                        std::make_unique<MediaRemotingDialogCoordinator>()) {
    cast_connector_.set_remoting_allowed_for_testing(true);
  }

  void TearDown() override {
    // Bridges are owned by their message pipes, which close as each test's
    // sources go out of scope; let those destruction tasks run while the
    // connectors they deregister from are still alive.
    EXPECT_TRUE(
        base::test::RunUntil([this] { return connector_.bridges_.empty(); }));
  }

 protected:
  FakeRedirectionSessionFactory& host() { return host_; }

  FakeRemoter* cast_remoter() { return cast_remoter_.get(); }

  // Creates a source served by a RemotingBridge, as the render process would.
  MockRemotingSource CreateSource() {
    return MockRemotingSource(&cast_connector_, &connector_);
  }

  // Settles every message in flight between |sources| and the sessions serving
  // them. A call travels source -> bridge -> session and the notification it
  // provokes travels back, so the pipes are flushed in that order: flushing one
  // lets the endpoint behind it run and write into the next.
  void FlushPipes(std::initializer_list<MockRemotingSource*> sources) {
    for (MockRemotingSource* source : sources) {
      source->FlushRemoterPipe();
    }
    for (size_t i = 0; i < host_.remoter_count(); ++i) {
      // Null once DropSession() has torn this session down.
      if (FakeRemoter* session = host_.remoter_at(i)) {
        session->FlushRemoterPipe();
        session->FlushSourcePipe();
      }
    }
    if (cast_remoter_) {
      cast_remoter_->FlushRemoterPipe();
      cast_remoter_->FlushSourcePipe();
    }
    for (MockRemotingSource* source : sources) {
      source->FlushSourcePipe();
    }
  }

  void StartRedirection() {
    connector_.StartingRedirection(host_.GetCallback());
  }

  void StopRedirection() { connector_.StoppingRedirection(); }

  // Simulates the Mirroring Service connecting a Cast session to this tab.
  void StartCastSession() {
    mojo::PendingRemote<media::mojom::Remoter> remoter;
    mojo::PendingRemote<media::mojom::RemotingSource> source;
    auto source_receiver = source.InitWithNewPipeAndPassReceiver();
    cast_remoter_ = std::make_unique<FakeRemoter>(
        remoter.InitWithNewPipeAndPassReceiver(), std::move(source));
    cast_connector_.ConnectWithMediaRemoter(std::move(remoter),
                                            std::move(source_receiver));
  }

  // Dropping the Mirroring Service end of the session closes the pipe, which is
  // how the Cast session ends in production. The connector only learns of it
  // once the disconnect handler runs.
  void StopCastSession() {
    cast_remoter_.reset();
    EXPECT_TRUE(base::test::RunUntil(
        [this] { return !cast_connector_.remoter_.is_bound(); }));
  }

  // How many registered bridges are currently served by a redirection session.
  size_t RedirectingCount() const {
    size_t count = 0;
    for (const auto& [bridge, source_bridge] : connector_.bridges_) {
      if (source_bridge) {
        ++count;
      }
    }
    return count;
  }

 private:
  content::BrowserTaskEnvironment task_environment_;
  sync_preferences::TestingPrefServiceSyncable pref_service_;
  CastRemotingConnector cast_connector_;
  RedirectionConnector connector_;
  FakeRedirectionSessionFactory host_;
  std::unique_ptr<FakeRemoter> cast_remoter_;
};

// -- Session lifecycle --------------------------------------------------

TEST_F(RedirectionConnectorTest, DoesNotRedirectBeforeRedirectionStarts) {
  MockRemotingSource source = CreateSource();
  EXPECT_EQ(RedirectingCount(), 0u);
}

TEST_F(RedirectionConnectorTest, RedirectsBridgeRegisteredBeforeStart) {
  MockRemotingSource source = CreateSource();

  StartRedirection();
  EXPECT_EQ(RedirectingCount(), 1u);
  EXPECT_EQ(host().remoter_count(), 1u);
}

TEST_F(RedirectionConnectorTest, RedirectsBridgeRegisteredAfterStart) {
  StartRedirection();

  MockRemotingSource source = CreateSource();
  EXPECT_EQ(RedirectingCount(), 1u);
}

TEST_F(RedirectionConnectorTest, StoppingRedirectionReleasesBridge) {
  MockRemotingSource source = CreateSource();
  StartRedirection();
  ASSERT_EQ(RedirectingCount(), 1u);

  StopRedirection();
  EXPECT_EQ(RedirectingCount(), 0u);
}

// Ensure Redirection creates one session per source, so several sources may
// remote at once.
TEST_F(RedirectionConnectorTest, GivesEverySourceItsOwnSession) {
  MockRemotingSource source1 = CreateSource();
  MockRemotingSource source2 = CreateSource();

  StartRedirection();
  EXPECT_EQ(RedirectingCount(), 2u);
  ASSERT_EQ(host().remoter_count(), 2u);

  // Both may be remoting simultaneously; neither is refused for being second.
  EXPECT_CALL(source1, OnStarted()).Times(1);
  EXPECT_CALL(source2, OnStarted()).Times(1);
  EXPECT_CALL(source1, OnStartFailed(_)).Times(0);
  EXPECT_CALL(source2, OnStartFailed(_)).Times(0);
  source1.remoter()->Start();
  source2.remoter()->Start();
  FlushPipes({&source1, &source2});

  EXPECT_EQ(host().remoter_at(0)->start_count(), 1);
  EXPECT_EQ(host().remoter_at(1)->start_count(), 1);
}

// -- Sink availability --------------------------------------------------

TEST_F(RedirectionConnectorTest, NeverNotifiesThatSinkIsAvailable) {
  MockRemotingSource source = CreateSource();

  EXPECT_CALL(source, OnSinkAvailable()).Times(0);
  EXPECT_CALL(source, OnSinkGone()).Times(AtLeast(0));
  FlushPipes({&source});
}

// The session, not the connector, decides when it is ready to serve media.
TEST_F(RedirectionConnectorTest, NotifiesWhenSinkIsAvailableAndThenGone) {
  MockRemotingSource source = CreateSource();
  StartRedirection();

  EXPECT_CALL(source, OnSinkAvailable()).Times(1);
  host().remoter_at(0)->OnSinkAvailable();
  FlushPipes({&source});

  // The source never started remoting, so it is owed no OnStopped().
  EXPECT_CALL(source, OnStopped(_)).Times(0);
  EXPECT_CALL(source, OnSinkGone()).Times(1);
  StopRedirection();
  FlushPipes({&source});
}

TEST_F(RedirectionConnectorTest,
       NotifiesMultipleSourcesWhenSinkIsAvailableAndThenGone) {
  MockRemotingSource source1 = CreateSource();
  MockRemotingSource source2 = CreateSource();
  StartRedirection();
  ASSERT_EQ(host().remoter_count(), 2u);

  EXPECT_CALL(source1, OnSinkAvailable()).Times(1);
  EXPECT_CALL(source2, OnSinkAvailable()).Times(1);
  host().remoter_at(0)->OnSinkAvailable();
  host().remoter_at(1)->OnSinkAvailable();
  FlushPipes({&source1, &source2});

  EXPECT_CALL(source1, OnSinkGone()).Times(1);
  EXPECT_CALL(source2, OnSinkGone()).Times(1);
  EXPECT_CALL(source1, OnStopped(_)).Times(0);
  EXPECT_CALL(source2, OnStopped(_)).Times(0);
  StopRedirection();
  FlushPipes({&source1, &source2});
}

TEST_F(RedirectionConnectorTest,
       AddsRenderingFeatureToTheSessionsSinkMetadata) {
  MockRemotingSource source = CreateSource();
  StartRedirection();
  ASSERT_EQ(RedirectingCount(), 1u);

  host().remoter_at(0)->OnSinkAvailable();
  FlushPipes({&source});

  ASSERT_TRUE(source.last_sink_metadata());
  EXPECT_THAT(source.last_sink_metadata()->features,
              testing::Contains(media::mojom::RemotingSinkFeature::RENDERING));
}

// -- Starting and relaying ----------------------------------------------

TEST_F(RedirectionConnectorTest, NoRedirectionSession) {
  MockRemotingSource source = CreateSource();

  EXPECT_CALL(source,
              OnStartFailed(RemotingStartFailReason::INVALID_ANSWER_MESSAGE))
      .Times(1);
  source.remoter()->Start();
  FlushPipes({&source});
}

TEST_F(RedirectionConnectorTest, RelaysMessagesInBothDirections) {
  MockRemotingSource source = CreateSource();
  StartRedirection();

  EXPECT_CALL(source, OnStarted()).Times(1);
  source.remoter()->Start();
  FlushPipes({&source});

  const std::vector<uint8_t> message_to_sink = {3, 1, 4, 1, 5, 9};
  source.remoter()->SendMessageToSink(message_to_sink);
  FlushPipes({&source});
  EXPECT_THAT(host().remoter_at(0)->messages_to_sink(),
              testing::ElementsAre(message_to_sink));

  const std::vector<uint8_t> message_to_source = {2, 7, 1, 8, 2, 8};
  EXPECT_CALL(source, OnMessageFromSink(message_to_source)).Times(1);
  host().remoter_at(0)->OnMessageFromSink(message_to_source);
  FlushPipes({&source});
}

// -- Stopping -----------------------------------------------------------

TEST_F(RedirectionConnectorTest, SourceInitiatedStopTearsDownTheSession) {
  MockRemotingSource source = CreateSource();
  StartRedirection();

  EXPECT_CALL(source, OnStarted()).Times(1);
  source.remoter()->Start();
  FlushPipes({&source});

  const RemotingStopReason reason = RemotingStopReason::LOCAL_PLAYBACK;
  EXPECT_CALL(source, OnSinkGone()).Times(AtLeast(1));
  EXPECT_CALL(source, OnStopped(reason)).Times(1);
  source.remoter()->Stop(reason);
  FlushPipes({&source});
  EXPECT_THAT(host().remoter_at(0)->stop_reasons(),
              testing::ElementsAre(reason));
}

// The session reporting failure stops that source, and only that source.
TEST_F(RedirectionConnectorTest, SessionReportedFailureStopsOnlyThatSource) {
  StartRedirection();
  MockRemotingSource source1 = CreateSource();
  MockRemotingSource source2 = CreateSource();
  ASSERT_EQ(host().remoter_count(), 2u);

  EXPECT_CALL(source1, OnStarted()).Times(1);
  EXPECT_CALL(source2, OnStarted()).Times(1);
  source1.remoter()->Start();
  source2.remoter()->Start();
  FlushPipes({&source1, &source2});

  EXPECT_CALL(source2, OnStopped(_)).Times(0);
  EXPECT_CALL(source2, OnSinkGone()).Times(0);
  EXPECT_CALL(source1, OnSinkGone()).Times(1);
  EXPECT_CALL(source1, OnStopped(RemotingStopReason::UNEXPECTED_FAILURE))
      .Times(1);
  host().remoter_at(0)->OnStopped(RemotingStopReason::UNEXPECTED_FAILURE);
  FlushPipes({&source1, &source2});
}

TEST_F(RedirectionConnectorTest, SessionReportedSinkGoneStopsRemoting) {
  MockRemotingSource source = CreateSource();
  StartRedirection();

  EXPECT_CALL(source, OnSinkAvailable()).Times(1);
  host().remoter_at(0)->OnSinkAvailable();
  FlushPipes({&source});

  EXPECT_CALL(source, OnStarted()).Times(1);
  source.remoter()->Start();
  FlushPipes({&source});

  EXPECT_CALL(source, OnSinkGone()).Times(1);
  EXPECT_CALL(source, OnStopped(RemotingStopReason::SERVICE_GONE)).Times(1);
  host().remoter_at(0)->OnSinkGone();
  FlushPipes({&source});
}

// A source that never started remoting is told the sink is gone, but not that
// a session it does not believe it is in has stopped.
TEST_F(RedirectionConnectorTest, DoesNotReportStoppedWhenRemotingNeverStarted) {
  MockRemotingSource source = CreateSource();
  StartRedirection();

  EXPECT_CALL(source, OnSinkAvailable()).Times(1);
  host().remoter_at(0)->OnSinkAvailable();
  FlushPipes({&source});

  // The session survives this, so it may report the sink gone again when it is
  // torn down; what matters is that the source is never told it stopped.
  EXPECT_CALL(source, OnSinkGone()).Times(AtLeast(1));
  EXPECT_CALL(source, OnStopped(_)).Times(0);
  host().remoter_at(0)->OnSinkGone();
  FlushPipes({&source});
}

// Stopping is reported once, not again when the session is later torn down.
TEST_F(RedirectionConnectorTest, ReportsStoppedOnlyOnce) {
  MockRemotingSource source = CreateSource();
  StartRedirection();

  EXPECT_CALL(source, OnStarted()).Times(1);
  source.remoter()->Start();
  FlushPipes({&source});

  EXPECT_CALL(source, OnStopped(RemotingStopReason::LOCAL_PLAYBACK)).Times(1);
  source.remoter()->Stop(RemotingStopReason::LOCAL_PLAYBACK);
  FlushPipes({&source});

  EXPECT_CALL(source, OnStopped(_)).Times(0);
  StopRedirection();
  FlushPipes({&source});
}

// The utility process can drop one source's session without ending redirection
// for the others, so the source is told why rather than left waiting on a
// session that no longer exists.
TEST_F(RedirectionConnectorTest, SessionDisconnectStopsOnlyThatSource) {
  StartRedirection();
  MockRemotingSource source1 = CreateSource();
  MockRemotingSource source2 = CreateSource();
  ASSERT_EQ(host().remoter_count(), 2u);

  EXPECT_CALL(source1, OnStarted()).Times(1);
  EXPECT_CALL(source2, OnStarted()).Times(1);
  source1.remoter()->Start();
  source2.remoter()->Start();
  FlushPipes({&source1, &source2});

  EXPECT_CALL(source1, OnSinkGone()).Times(AtLeast(1));
  EXPECT_CALL(source1, OnStopped(RemotingStopReason::UNEXPECTED_FAILURE))
      .Times(1);
  EXPECT_CALL(source2, OnSinkGone()).Times(0);
  EXPECT_CALL(source2, OnStopped(_)).Times(0);
  host().DropSession(0);
  EXPECT_TRUE(
      base::test::RunUntil([this] { return RedirectingCount() == 1u; }));
  FlushPipes({&source1, &source2});
}

// -- Teardown ordering --------------------------------------------------

TEST_F(RedirectionConnectorTest, HandlesTeardownOfRemotingSourceFirst) {
  MockRemotingSource source = CreateSource();
  StartRedirection();

  EXPECT_CALL(source, OnSinkAvailable()).Times(1);
  host().remoter_at(0)->OnSinkAvailable();
  FlushPipes({&source});

  // The bridge outlives this, so nothing observable changes; TearDown() drains
  // the disconnect that reaches it.
  source.CloseSourcePipe();
}

TEST_F(RedirectionConnectorTest, HandlesTeardownOfRemoterFirst) {
  MockRemotingSource source = CreateSource();
  StartRedirection();

  EXPECT_CALL(source, OnSinkAvailable()).Times(1);
  host().remoter_at(0)->OnSinkAvailable();
  FlushPipes({&source});

  EXPECT_CALL(source, OnSinkGone()).Times(AtLeast(0));
  EXPECT_CALL(source, OnStopped(_)).Times(AtLeast(0));
  // Dropping the Remoter destroys the bridge, which deregisters itself.
  source.CloseRemoterPipe();
  EXPECT_TRUE(base::test::RunUntil([this] { return RedirectingCount() == 0; }));
}

// -- Arbitration with the Cast path -------------------------------------

// Casting a tab takes over from redirection: both take over the same element's
// media pipeline, and MMR paints via the RDP client rather than into the tab's
// composited output that the Cast session mirrors.
TEST_F(RedirectionConnectorTest, CastSessionStopsRedirection) {
  MockRemotingSource source = CreateSource();
  StartRedirection();
  ASSERT_EQ(RedirectingCount(), 1u);

  StartCastSession();
  EXPECT_EQ(RedirectingCount(), 0u);
}

TEST_F(RedirectionConnectorTest, RedirectionResumesWhenCastSessionEnds) {
  MockRemotingSource source = CreateSource();
  StartRedirection();
  StartCastSession();
  ASSERT_EQ(RedirectingCount(), 0u);

  StopCastSession();
  EXPECT_EQ(RedirectingCount(), 1u);
}

// Redirection ending is invisible to a tab that is being cast: the source is
// already on the cast path, and stays there.
TEST_F(RedirectionConnectorTest, StoppingRedirectionLeavesCastSessionAlone) {
  MockRemotingSource source = CreateSource();
  StartRedirection();
  StartCastSession();

  EXPECT_CALL(source, OnSinkAvailable()).Times(1);
  cast_remoter()->OnSinkAvailable();
  FlushPipes({&source});

  EXPECT_CALL(source, OnStarted()).Times(1);
  source.remoter()->Start();
  FlushPipes({&source});

  EXPECT_CALL(source, OnSinkGone()).Times(0);
  EXPECT_CALL(source, OnStopped(_)).Times(0);
  StopRedirection();
  FlushPipes({&source});
  EXPECT_THAT(cast_remoter()->stop_reasons(), testing::IsEmpty());
}

// A tab that is already being cast when redirection starts is skipped rather
// than tripping over the "no session yet" expectation.
TEST_F(RedirectionConnectorTest, DoesNotRedirectTabAlreadyBeingCast) {
  MockRemotingSource source = CreateSource();
  StartCastSession();

  StartRedirection();
  EXPECT_EQ(RedirectingCount(), 0u);
  EXPECT_EQ(host().remoter_count(), 0u);
}

// A bridge created in a tab that is already being cast stays on the cast path.
TEST_F(RedirectionConnectorTest,
       DoesNotRedirectBridgeCreatedDuringCastSession) {
  StartRedirection();
  StartCastSession();

  MockRemotingSource source = CreateSource();
  EXPECT_EQ(RedirectingCount(), 0u);
}
