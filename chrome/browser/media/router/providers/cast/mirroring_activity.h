// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_ROUTER_PROVIDERS_CAST_MIRRORING_ACTIVITY_H_
#define CHROME_BROWSER_MEDIA_ROUTER_PROVIDERS_CAST_MIRRORING_ACTIVITY_H_

#include <string>

#include "base/functional/callback.h"
#include "base/gtest_prod_util.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "chrome/browser/media/mirroring_service_host.h"
#include "chrome/browser/media/router/providers/cast/cast_activity.h"
#include "chrome/browser/media/router/providers/cast/cast_session_tracker.h"
#include "components/media_router/common/media_route.h"
#include "components/media_router/common/mojom/logger.mojom.h"
#include "components/media_router/common/mojom/media_router.mojom-forward.h"
#include "components/media_router/common/providers/cast/channel/cast_message_handler.h"
#include "components/mirroring/mojom/cast_message_channel.mojom.h"
#include "components/mirroring/mojom/session_observer.mojom.h"
#include "components/mirroring/mojom/session_parameters.mojom-forward.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/openscreen/src/cast/common/channel/proto/cast_channel.pb.h"

namespace media_router {

struct CastSinkExtraData;

class MirroringActivity : public CastActivity,
                          public mirroring::mojom::SessionObserver,
                          public mirroring::mojom::CastMessageChannel {
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
                    int frame_tree_node_id,
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

  // CastMessageChannel implementation
  void OnMessage(mirroring::mojom::CastMessagePtr message) override;

  // CastActivity implementation
  void OnAppMessage(const cast::channel::CastMessage& message) override;
  void OnInternalMessage(const cast_channel::InternalMessage& message) override;

  mirroring::MirroringServiceHost* GetHost() { return host_.get(); }
  void SetMirroringServiceHostForTest(
      std::unique_ptr<mirroring::MirroringServiceHost> host) {
    host_ = std::move(host);
  }

 protected:
  void OnSessionSet(const CastSession& session) override;
  void CreateMediaController(
      mojo::PendingReceiver<mojom::MediaController> media_controller,
      mojo::PendingRemote<mojom::MediaStatusObserver> observer) override;
  std::string GetRouteDescription(const CastSession& session) const override;

 private:
  FRIEND_TEST_ALL_PREFIXES(MirroringActivityTest, GetScrubbedLogMessage);
  FRIEND_TEST_ALL_PREFIXES(MirroringActivityTest, OnSourceChanged);

  void HandleParseJsonResult(const std::string& route_id,
                             data_decoder::DataDecoder::ValueOrError result);

  void StopMirroring();

  void set_host(std::unique_ptr<mirroring::MirroringServiceHost> host) {
    host_ = std::move(host);
  }

  // Scrubs AES related data in messages with type "OFFER".
  static std::string GetScrubbedLogMessage(const base::Value::Dict& message);

  // Starts the mirroring service via the Ui thread. Can only be called on the
  // Ui thread.
  void StartOnUiThread(
      base::WeakPtr<mirroring::MirroringServiceHost> host,
      mirroring::mojom::SessionParametersPtr session_params,
      mojo::PendingRemote<mirroring::mojom::SessionObserver> observer,
      mojo::PendingRemote<mirroring::mojom::CastMessageChannel>
          outbound_channel,
      mojo::PendingReceiver<mirroring::mojom::CastMessageChannel>
          inbound_channel,
      const std::string& sink_name,
      int frame_tree_node_id);

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

  mojo::Receiver<mirroring::mojom::SessionObserver> observer_receiver_{this};

  // To handle Cast messages from the mirroring service to the mirroring
  // receiver.
  mojo::Receiver<mirroring::mojom::CastMessageChannel> channel_receiver_{this};

  // Set before and after a mirroring session is established, for metrics.
  absl::optional<base::Time> will_start_mirroring_timestamp_;
  absl::optional<base::Time> did_start_mirroring_timestamp_;

  const absl::optional<MirroringType> mirroring_type_;

  // The FrameTreeNode ID to retrieve the WebContents of the tab to mirror.
  int frame_tree_node_id_;
  const CastSinkExtraData cast_data_;
  OnStopCallback on_stop_;
  OnSourceChangedCallback source_changed_callback_;
  base::WeakPtrFactory<MirroringActivity> weak_ptr_factory_{this};
};

}  // namespace media_router

#endif  // CHROME_BROWSER_MEDIA_ROUTER_PROVIDERS_CAST_MIRRORING_ACTIVITY_H_
