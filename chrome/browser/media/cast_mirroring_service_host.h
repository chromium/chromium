// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_CAST_MIRRORING_SERVICE_HOST_H_
#define CHROME_BROWSER_MEDIA_CAST_MIRRORING_SERVICE_HOST_H_

#include <memory>
#include <string>

#include "base/gtest_prod_util.h"
#include "base/task/sequenced_task_runner.h"
#include "chrome/browser/media/offscreen_tab.h"
#include "components/mirroring/mojom/mirroring_service.mojom.h"
#include "components/mirroring/mojom/mirroring_service_host.mojom.h"
#include "components/mirroring/mojom/resource_provider.mojom.h"
#include "content/public/browser/desktop_media_id.h"
#include "content/public/browser/media_stream_request.h"
#include "content/public/browser/web_contents_observer.h"
#include "extensions/buildflags/buildflags.h"
#include "media/mojo/mojom/audio_stream_factory.mojom.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "ui/gfx/geometry/size.h"

namespace base {
class UnguessableToken;
}

namespace content {
class BrowserContext;
struct DesktopMediaID;
class WebContents;
}  // namespace content

namespace viz {
class GpuClient;
}

namespace mirroring {

// CastMirroringServiceHost starts/stops a mirroring session through Mirroring
// Service, and provides the resources to the Mirroring Service as requested.
class CastMirroringServiceHost final : public mojom::MirroringServiceHost,
                                       public mojom::ResourceProvider,
                                       public OffscreenTab::Owner,
                                       public content::WebContentsObserver {
 public:
  static void GetForTab(
      content::WebContents* target_contents,
      mojo::PendingReceiver<mojom::MirroringServiceHost> receiver);

  static void GetForDesktop(
      const content::DesktopMediaID& media_id,
      mojo::PendingReceiver<mojom::MirroringServiceHost> receiver);

  static void GetForOffscreenTab(
      content::BrowserContext* context,
      const GURL& presentation_url,
      const std::string& presentation_id,
      mojo::PendingReceiver<mojom::MirroringServiceHost> receiver);

  // |source_media_id| indicates the mirroring source.
  explicit CastMirroringServiceHost(content::DesktopMediaID source_media_id);

  CastMirroringServiceHost(const CastMirroringServiceHost&) = delete;
  CastMirroringServiceHost& operator=(const CastMirroringServiceHost&) = delete;

  ~CastMirroringServiceHost() override;

  // mojom::MirroringServiceHost implementation.
  void Start(mojom::SessionParametersPtr session_params,
             mojo::PendingRemote<mojom::SessionObserver> observer,
             mojo::PendingRemote<mojom::CastMessageChannel> outbound_channel,
             mojo::PendingReceiver<mojom::CastMessageChannel> inbound_channel)
      override;
  void GetTabSourceId(
      GetTabSourceIdCallback get_tab_source_id_callback) override;

 private:
  friend class CastMirroringServiceHostBrowserTest;
  FRIEND_TEST_ALL_PREFIXES(CastMirroringServiceHostTest,
                           TestGetClampedResolution);
  friend class CastV2PerformanceTest;
  FRIEND_TEST_ALL_PREFIXES(CastV2PerformanceTest, Performance);

  static gfx::Size GetCaptureResolutionConstraint();
  // Clamp resolution constraint to the screen size.
  static gfx::Size GetClampedResolution(gfx::Size screen_resolution);

  // ResourceProvider implementation.
  void BindGpu(mojo::PendingReceiver<viz::mojom::Gpu> receiver) override;
  void GetVideoCaptureHost(
      mojo::PendingReceiver<media::mojom::VideoCaptureHost> receiver) override;
  void GetNetworkContext(
      mojo::PendingReceiver<network::mojom::NetworkContext> receiver) override;
  void CreateAudioStream(
      mojo::PendingRemote<mojom::AudioStreamCreatorClient> requestor,
      const media::AudioParameters& params,
      uint32_t total_segments) override;
  void ConnectToRemotingSource(
      mojo::PendingRemote<media::mojom::Remoter> remoter,
      mojo::PendingReceiver<media::mojom::RemotingSource> receiver) override;

  void CreateAudioStreamForTab(
      mojo::PendingRemote<mojom::AudioStreamCreatorClient> requestor,
      const media::AudioParameters& params,
      uint32_t total_segments,
      const base::UnguessableToken& group_id);
  void CreateAudioStreamForDesktop(
      mojo::PendingRemote<mojom::AudioStreamCreatorClient> requestor,
      const media::AudioParameters& params,
      uint32_t total_segments);

  // content::WebContentsObserver implementation.
  void WebContentsDestroyed() override;

  // Registers the media stream to show a capture indicator icon on the
  // tabstrip.
  void ShowCaptureIndicator();

  // Registers the media stream to show source tab switching UI and a capture
  // indicator icon on the tabstrip.
  void ShowTabSharingUI(const blink::mojom::StreamDevices& devices);

  void SwitchMirroringSourceTab(const content::DesktopMediaID& media_id);

  // Records metrics about the usage of Tab Switcher UI, and resets data members
  // used for metrics collection.
  void RecordTabUIUsageMetricsIfNeededAndReset();

#if BUILDFLAG(ENABLE_EXTENSIONS)
  // OffscreenTab::Owner implementation.
  void DestroyTab(OffscreenTab* tab) override;

  // Creates and starts a new OffscreenTab.
  void OpenOffscreenTab(content::BrowserContext* context,
                        const GURL& presentation_url,
                        const std::string& presentation_id);
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

  // Describes the media source for this mirroring session.
  content::DesktopMediaID source_media_id_;

  // The receiver to this mojom::ResourceProvider implementation.
  mojo::Receiver<mojom::ResourceProvider> resource_provider_receiver_{this};

  // Connection to the remote mojom::MirroringService implementation.
  mojo::Remote<mojom::MirroringService> mirroring_service_;

  // The GpuClient associated with the Mirroring Service's GPU connection, if
  // any.
  std::unique_ptr<viz::GpuClient, base::OnTaskRunnerDeleter> gpu_client_;

  // Used to create WebContents loopback capture streams, or system-wide desktop
  // capture streams, from the Audio Service.
  mojo::Remote<media::mojom::AudioStreamFactory> audio_stream_factory_;

  // Used to mute local audio from the WebContents being mirrored (in the tab
  // mirrorng case). See the comments in the implementation of
  // CreateAudioStream() for further explanation.
  mojo::AssociatedRemote<media::mojom::LocalMuter> web_contents_audio_muter_;

  // The lifetime of the capture indicator icon on the tabstrip is tied to that
  // of |media_stream_ui_|.
  std::unique_ptr<content::MediaStreamUI> media_stream_ui_;

#if BUILDFLAG(ENABLE_EXTENSIONS)
  std::unique_ptr<OffscreenTab> offscreen_tab_;
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

  const bool tab_switching_ui_enabled_;

  // Represents the number of times a tab was switched during an active
  // mirroring session using tab switcher UI bar. Mainly used for metrics
  // collecting.
  absl::optional<int> tab_switching_count_;

  // Used for calls supplied to `media_stream_ui_`, mainly to handle callbacks
  // for TabSharingUIViews. Invalidated every time a new UI is created.
  base::WeakPtrFactory<CastMirroringServiceHost> weak_factory_for_ui_{this};
};

}  // namespace mirroring

#endif  // CHROME_BROWSER_MEDIA_CAST_MIRRORING_SERVICE_HOST_H_
