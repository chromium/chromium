// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_ROUTER_PROVIDERS_CAST_MIRRORING_ACTIVITY_H_
#define CHROME_BROWSER_MEDIA_ROUTER_PROVIDERS_CAST_MIRRORING_ACTIVITY_H_

#include <optional>
#include <string>

#include "base/functional/callback.h"
#include "base/gtest_prod_util.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/thread_annotations.h"
#include "base/time/time.h"
#include "chrome/browser/media/mirroring_service_host.h"
#include "chrome/browser/media/router/providers/cast/cast_activity.h"
#include "chrome/browser/media/router/providers/cast/cast_session_tracker.h"
#include "components/media_router/common/media_route.h"
#include "components/media_router/common/mojom/debugger.mojom.h"
#include "components/media_router/common/mojom/logger.mojom.h"
#include "components/media_router/common/mojom/media_controller.mojom.h"
#include "components/media_router/common/mojom/media_router.mojom-forward.h"
#include "components/media_router/common/mojom/media_status.mojom.h"
#include "components/media_router/common/providers/cast/channel/cast_message_handler.h"
#include "components/mirroring/mojom/cast_message_channel.mojom.h"
#include "components/mirroring/mojom/session_observer.mojom.h"
#include "components/mirroring/mojom/session_parameters.mojom-forward.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/remote_set.h"
#include "third_party/openscreen/src/cast/common/channel/proto/cast_channel.pb.h"

namespace media_router {

struct CastSinkExtraData;

class MirroringActivity : public CastActivity,
                          public mirroring::mojom::SessionObserver,
                          public mirroring::mojom::CastMessageChannel,
                          public mojom::MediaController {
 public:
  using OnStopCallback = base::OnceClosure;

  enum class MirroringType {
    kTab,           // Mirror a single tab.
    kDesktop,       // Mirror the whole desktop.
    kOffscreenTab,  // Used for Presentation API 1UA mode.
    kMaxValue = kOffscreenTab,
  };

  MirroringActivity(const MediaRoute& route,
                    const std::string& app_id,
                    cast_channel::CastMessageHandler* message_handler,
                    CastSessionTracker* session_tracker,
                    content::FrameTreeNodeId frame_tree_node_id,
                    const CastSinkExtraData& cast_data,
                    OnStopCallback callback,
                    OnSourceChangedCallback source_changed_callback);
  ~MirroringActivity() override;

  virtual void CreateMojoBindings(mojom::MediaRouter* media_router);

  // `host_factory_for_test` is made as a default parameter. It is only passed
  // when testing, otherwise it is initialized within the function itself.
  void CreateMirroringServiceHost(
      mirroring::MirroringServiceHostFactory* host_factory_for_test = nullptr);

  // SessionObserver implementation
  void OnError(mirroring::mojom::SessionError error) override;
  void DidStart() override;
  void DidStop() override;
  void LogInfoMessage(const std::string& message) override;
  void LogErrorMessage(const std::string& message) override;
  void OnSourceChanged() override;
  void OnRemotingStateChanged(bool is_remoting) override;

  // CastMessageChannel implementation
  void OnMessage(mirroring::mojom::CastMessagePtr message) override;

  // CastActivity implementation
  void OnAppMessage(
      const openscreen::cast::proto::CastMessage& message) override;
  void OnInternalMessage(const cast_channel::InternalMessage& message) override;

  // mojom::MediaController implementation
  void SetMute(bool mute) override {}
  void SetVolume(float volume) override {}
  void Seek(base::TimeDelta time) override {}
  void NextTrack() override {}
  void PreviousTrack() override {}
  // Used during access code casting to resume the mirroring session, if
  // frozen. This will send a request to the video capture host to resume
  // displaying the casting session.
  void Play() override;
  // Used during access code casting to freeze the mirroring session. This will
  // send a request to the video capture host to pause the display of the
  // mirroring session.
  void Pause() override;

  mirroring::MirroringServiceHost* GetHost() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(ui_sequence_checker_);
    return host_.get();
  }
  void SetMirroringServiceHostForTest(
      std::unique_ptr<mirroring::MirroringServiceHost> host) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(ui_sequence_checker_);
    host_ = std::move(host);
  }

 protected:
  void OnSessionSet(const CastSession& session) override;
  void StartSession(const std::string& destination_id,
                    bool enable_rtcp_reporting = false);
  void BindMediaController(
      mojo::PendingReceiver<mojom::MediaController> media_controller,
      mojo::PendingRemote<mojom::MediaStatusObserver> observer) override;
  std::string GetRouteDescription(const CastSession& session) const override;

 private:
  FRIEND_TEST_ALL_PREFIXES(MirroringActivityTest, GetScrubbedLogMessage);
  FRIEND_TEST_ALL_PREFIXES(MirroringActivityTest, OnSourceChanged);
  FRIEND_TEST_ALL_PREFIXES(MirroringActivityTest,
                           OnSourceChangedNotifiesMediaStatusObserver);
  FRIEND_TEST_ALL_PREFIXES(MirroringActivityTest, ReportsNotEnabledByDefault);
  FRIEND_TEST_ALL_PREFIXES(MirroringActivityTest, EnableRtcpReports);
  FRIEND_TEST_ALL_PREFIXES(MirroringActivityTest, Pause);
  FRIEND_TEST_ALL_PREFIXES(MirroringActivityTest, Play);
  FRIEND_TEST_ALL_PREFIXES(MirroringActivityTest, OnRemotingStateChanged);
  FRIEND_TEST_ALL_PREFIXES(MirroringActivityTest,
                           MultipleMediaControllersNotified);

  void HandleParseJsonResult(const std::string& route_id,
                             data_decoder::DataDecoder::ValueOrError result);

  void StopMirroring();

  void set_host(std::unique_ptr<mirroring::MirroringServiceHost> host) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(ui_sequence_checker_);
    host_ = std::move(host);
  }

  void SetPlayState(mojom::MediaStatus::PlayState play_state);

  void NotifyMediaStatusObservers();

  // Invoked when mirroring is paused / resumed, for metrics.
  void OnMirroringPaused();
  void OnMirroringResumed();

  // Scrubs AES related data in messages with type "OFFER".
  static std::string GetScrubbedLogMessage(const base::Value::Dict& message);

  // Starts the mirroring service via the Ui thread. Can only be called on the
  // Ui thread.
  void StartOnUiThread(
      mirroring::mojom::SessionParametersPtr session_params,
      mojo::PendingRemote<mirroring::mojom::SessionObserver> observer,
      mojo::PendingRemote<mirroring::mojom::CastMessageChannel>
          outbound_channel,
      mojo::PendingReceiver<mirroring::mojom::CastMessageChannel>
          inbound_channel,
      const std::string& sink_name);

  void ScheduleFetchMirroringStats();
  void FetchMirroringStats();
  void OnMirroringStats(base::Value json_stats);

  std::unique_ptr<mirroring::MirroringServiceHost> host_;

  // Sends Cast messages from the mirroring receiver to the mirroring service.
  mojo::Remote<mirroring::mojom::CastMessageChannel> channel_to_service_;

  // Only used to store pending CastMessageChannel receiver while waiting for
  // OnSessionSet() to be called.
  mojo::PendingReceiver<mirroring::mojom::CastMessageChannel>
      channel_to_service_receiver_;

  // Remote to the logger owned by the Media Router. Used to log WebRTC messages
  // sent between the mirroring service and mirroring receiver.
  // |logger_| should be bound before the CastMessageChannel message pipe is
  // created.
  mojo::Remote<mojom::Logger> logger_;

  // Remote to the debugger owned by the Media Router. Used to check if
  // mirroring stats are enabled on the mirroring session and to receive
  // mirroring stats from the session.
  mojo::Remote<mojom::Debugger> debugger_;

  // Most recent fetched mirroring stats.
  base::Value::Dict most_recent_mirroring_stats_;

  mojo::Receiver<mirroring::mojom::SessionObserver> observer_receiver_{this};

  // To handle Cast messages from the mirroring service to the mirroring
  // receiver.
  mojo::Receiver<mirroring::mojom::CastMessageChannel> channel_receiver_{this};

  // To handle freeze and unfreeze requests from media controllers.
  mojo::ReceiverSet<mojom::MediaController> media_controller_receivers_;

  // Sends media status updates with mirroring information to observers.
  mojo::RemoteSet<mojom::MediaStatusObserver> media_status_observers_;

  // Info for mirroring state transitions like pause / resume.
  mojom::MediaStatusPtr media_status_;
  int mirroring_pause_count_ = 0;
  std::optional<base::Time> mirroring_pause_timestamp_;

  // Set before and after a mirroring session is established, for metrics.
  std::optional<base::Time> will_start_mirroring_timestamp_;
  std::optional<base::Time> did_start_mirroring_timestamp_;

  const std::optional<MirroringType> mirroring_type_;

  std::optional<base::TimeDelta> target_playout_delay_;

  // The FrameTreeNode ID to retrieve the WebContents of the tab to mirror.
  content::FrameTreeNodeId frame_tree_node_id_;
  const CastSinkExtraData cast_data_;
  OnStopCallback on_stop_;
  OnSourceChangedCallback source_changed_callback_;

  bool should_fetch_stats_on_start_ = false;

  SEQUENCE_CHECKER(io_sequence_checker_);
  SEQUENCE_CHECKER(ui_sequence_checker_);
  base::WeakPtrFactory<MirroringActivity> weak_ptr_factory_{this};
};

}  // namespace media_router

#endif  // CHROME_BROWSER_MEDIA_ROUTER_PROVIDERS_CAST_MIRRORING_ACTIVITY_H_
