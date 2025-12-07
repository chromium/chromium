// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_MIRRORING_SERVICE_HOST_H_
#define CHROME_BROWSER_MEDIA_MIRRORING_SERVICE_HOST_H_

#include "components/mirroring/mojom/mirroring_service.mojom.h"
#include "content/public/browser/frame_tree_node_id.h"
#include "url/gurl.h"

namespace mirroring {

// This class is used as an interface for starting a mirroring session through
// the Mirroring Service. Must be called on UI thread.
class MirroringServiceHost {
 public:
  virtual ~MirroringServiceHost();

  MirroringServiceHost(const MirroringServiceHost&) = delete;
  MirroringServiceHost& operator=(const MirroringServiceHost&) = delete;

  // Starts a mirroring session through the Mirroring Service. |observer| gets
  // notifications about lifecycle events. |outbound_channel| is provided to
  // handle the messages to the mirroring receiver. |inbound_channel| receives
  // the messages from the mirroring receiver to the Mirroring Service.
  // To stop the session, just call Destructor.
  virtual void Start(
      mojom::SessionParametersPtr session_params,
      mojo::PendingRemote<mojom::SessionObserver> observer,
      mojo::PendingRemote<mojom::CastMessageChannel> outbound_channel,
      mojo::PendingReceiver<mojom::CastMessageChannel> inbound_channel,
      const std::string& sink_name) = 0;

  // Replies back with the current tab source id of the active mirroring
  // session if exist. Doesn't need to be called on UI thread.
  virtual std::optional<content::FrameTreeNodeId> GetTabSourceId() const = 0;

  // Fetches the statistics of the current mirroring sessions.
  virtual void GetMirroringStats(
      base::OnceCallback<void(const base::Value)> json_stats_cb) = 0;

  // Makes a call to the VideoCaptureHost to pause the current mirroring
  // session.
  virtual void Pause(base::OnceClosure on_paused_callback) = 0;

  // Makes a call to the VideoCaptureHost to resume the current mirroring
  // session.
  virtual void Resume(base::OnceClosure on_resumed_callback) = 0;

  base::WeakPtr<MirroringServiceHost> GetWeakPtr();

 protected:
  MirroringServiceHost();

 private:
  base::WeakPtrFactory<MirroringServiceHost> weak_factory_{this};
};

// Must be called on UI thread.
class MirroringServiceHostFactory {
 public:
  virtual ~MirroringServiceHostFactory();

  MirroringServiceHostFactory(const MirroringServiceHostFactory&) = delete;
  MirroringServiceHostFactory& operator=(const MirroringServiceHostFactory&) =
      delete;

  virtual std::unique_ptr<MirroringServiceHost> GetForTab(
      content::FrameTreeNodeId frame_tree_node_id) = 0;

  virtual std::unique_ptr<MirroringServiceHost> GetForDesktop(
      const std::optional<std::string>& media_id) = 0;

  virtual std::unique_ptr<MirroringServiceHost> GetForOffscreenTab(
      const GURL& presentation_url,
      const std::string& presentation_id,
      content::FrameTreeNodeId frame_tree_node_id) = 0;

 protected:
  MirroringServiceHostFactory();
};

}  // namespace mirroring

#endif  // CHROME_BROWSER_MEDIA_MIRRORING_SERVICE_HOST_H_
