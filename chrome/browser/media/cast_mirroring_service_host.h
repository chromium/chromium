// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_CAST_MIRRORING_SERVICE_HOST_H_
#define CHROME_BROWSER_MEDIA_CAST_MIRRORING_SERVICE_HOST_H_

#include <memory>
#include <string>

#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/sequenced_task_runner.h"
#include "components/mirroring/mojom/mirroring_service.mojom.h"
#include "components/mirroring/mojom/mirroring_service_host.mojom.h"
#include "components/mirroring/mojom/resource_provider.mojom.h"
#include "content/public/browser/desktop_media_id.h"
#include "content/public/browser/media_stream_request.h"
#include "content/public/browser/web_contents_observer.h"
#include "extensions/buildflags/buildflags.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "ui/gfx/geometry/size.h"

// TODO(crbug.com/879012): Remove the build flag. OffscreenTab should not only
// be defined when extension is enabled.
#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "chrome/browser/media/offscreen_tab.h"
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

namespace content {
class AudioLoopbackStreamCreator;
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
#if BUILDFLAG(ENABLE_EXTENSIONS)
                                       public OffscreenTab::Owner,
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)
                                       public content::WebContentsObserver {
 public:
  static void GetForTab(
      content::WebContents* target_contents,
      mojo::PendingReceiver<mojom::MirroringServiceHost> receiver);

  // TODO(crbug.com/809249): Remove when the extension-based implementation of
  // the Cast MRP is removed.
  static void GetForDesktop(
      content::WebContents* initiator_contents,
      const std::string& desktop_stream_id,
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

  ~CastMirroringServiceHost() override;

  // mojom::MirroringServiceHost implementation.
  void Start(mojom::SessionParametersPtr session_params,
             mojo::PendingRemote<mojom::SessionObserver> observer,
             mojo::PendingRemote<mojom::CastMessageChannel> outbound_channel,
             mojo::PendingReceiver<mojom::CastMessageChannel> inbound_channel)
      override;

 private:
  friend class CastMirroringServiceHostBrowserTest;
  FRIEND_TEST_ALL_PREFIXES(CastMirroringServiceHostTest,
                           TestGetClampedResolution);

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
      mojo::PendingRemote<mojom::AudioStreamCreatorClient> client,
      const media::AudioParameters& params,
      uint32_t total_segments) override;
  void ConnectToRemotingSource(
      mojo::PendingRemote<media::mojom::Remoter> remoter,
      mojo::PendingReceiver<media::mojom::RemotingSource> receiver) override;

  // content::WebContentsObserver implementation.
  void WebContentsDestroyed() override;

  // Registers the media stream to show a capture indicator icon on the
  // tabstrip.
  void ShowCaptureIndicator();

#if BUILDFLAG(ENABLE_EXTENSIONS)
  // OffscreenTab::Owner implementation.
  void RequestMediaAccessPermission(
      const content::MediaStreamRequest& request,
      content::MediaResponseCallback callback) override;
  void DestroyTab(OffscreenTab* tab) override;

  // Creates and starts a new OffscreenTab.
  void OpenOffscreenTab(content::BrowserContext* context,
                        const GURL& presentation_url,
                        const std::string& presentation_id);
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

  // Describes the media source for this mirroring session.
  content::DesktopMediaID source_media_id_;

  // The receiver to this mojom::ResourceProvider implementation.
  mojo::Receiver<mojom::ResourceProvider> resource_provider_receiver{this};

  // Connection to the remote mojom::MirroringService implementation.
  mojo::Remote<mojom::MirroringService> mirroring_service_;

  // The GpuClient associated with the Mirroring Service's GPU connection, if
  // any.
  std::unique_ptr<viz::GpuClient, base::OnTaskRunnerDeleter> gpu_client_;

  // Used to create an audio loopback stream through the Audio Service.
  std::unique_ptr<content::AudioLoopbackStreamCreator> audio_stream_creator_;

  // The lifetime of the capture indicator icon on the tabstrip is tied to that
  // of |media_stream_ui_|.
  std::unique_ptr<content::MediaStreamUI> media_stream_ui_;

#if BUILDFLAG(ENABLE_EXTENSIONS)
  std::unique_ptr<OffscreenTab> offscreen_tab_;
#endif  // BUILDFLAG(ENABLE_EXTENSIONS)

  DISALLOW_COPY_AND_ASSIGN(CastMirroringServiceHost);
};

}  // namespace mirroring

#endif  // CHROME_BROWSER_MEDIA_CAST_MIRRORING_SERVICE_HOST_H_
