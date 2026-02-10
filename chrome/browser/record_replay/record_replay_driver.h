// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_RECORD_REPLAY_RECORD_REPLAY_DRIVER_H_
#define CHROME_BROWSER_RECORD_REPLAY_RECORD_REPLAY_DRIVER_H_

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ref.h"
#include "chrome/common/record_replay/record_replay.mojom.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "third_party/blink/public/common/tokens/tokens.h"

namespace content {
class RenderFrameHost;
}

namespace record_replay {

class RecordReplayClient;

// The gateway for communication with the RenderReplayAgent in the renderer.
//
// Owned by RecordReplayDriverFactory.
class RecordReplayDriver : public mojom::RecordReplayDriver {
 public:
  RecordReplayDriver(content::RenderFrameHost* render_frame_host,
                     RecordReplayClient& client);
  RecordReplayDriver(const RecordReplayDriver&) = delete;
  RecordReplayDriver& operator=(const RecordReplayDriver&) = delete;
  ~RecordReplayDriver() override;

  void BindPendingReceiver(
      mojo::PendingAssociatedReceiver<mojom::RecordReplayDriver>
          pending_receiver);

  // Returns true if the driver's RenderFrameHost is active (i.e., neither
  // prerendered nor bfcached).
  bool IsActive() const;

  // Returns the unique identifier of the driver's RenderFrameHost.
  const blink::LocalFrameToken& GetFrameToken() const;

  // See mojom::RecordReplayAgent record_replay.mojom.
  void StartRecording();
  void StopRecording();
  void GetElementSelector(int64_t dom_node_id,
                          base::OnceCallback<void(const std::string&)> cb);
  void GetMatchingElements(
      const std::string& element_selector,
      base::OnceCallback<void(const std::vector<int64_t>&)> cb);
  void DoClick(int64_t dom_node_id, base::OnceCallback<void(bool)> cb);
  void DoPaste(int64_t dom_node_id,
               const std::string& text,
               base::OnceCallback<void(bool)> cb);
  void DoSelect(int64_t dom_node_id,
                const std::string& value,
                base::OnceCallback<void(bool)> cb);

  // mojom::RecordReplayDriver:
  void OnClick(int64_t dom_node_id,
               const std::string& element_selector) override;
  void OnSelectChanged(int64_t dom_node_id,
                       const std::string& element_selector,
                       const std::string& text) override;
  void OnTextChange(int64_t dom_node_id,
                    const std::string& element_selector,
                    const std::string& text) override;

 private:
  const raw_ref<RecordReplayClient> client_;
  const raw_ref<content::RenderFrameHost> rfh_;
  mojo::AssociatedReceiver<mojom::RecordReplayDriver> receiver_{this};
};

}  // namespace record_replay

#endif  // CHROME_BROWSER_RECORD_REPLAY_RECORD_REPLAY_DRIVER_H_
