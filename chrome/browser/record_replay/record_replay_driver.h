// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_RECORD_REPLAY_RECORD_REPLAY_DRIVER_H_
#define CHROME_BROWSER_RECORD_REPLAY_RECORD_REPLAY_DRIVER_H_

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ref.h"
#include "chrome/common/record_replay/aliases.h"
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
  // TODO(b/476101114): Remove once RecordReplayDriver has a
  // mojom::RecordReplayAgent.
  class TestRecordReplayAgent {
   public:
    virtual void StartRecording() = 0;
    virtual void StopRecording() = 0;
    virtual void GetElementSelector(DomNodeId dom_node_id,
                                    base::OnceCallback<void(Selector)> cb) = 0;
    virtual void GetMatchingElements(
        Selector element_selector,
        base::OnceCallback<void(const std::vector<DomNodeId>&)> cb) = 0;
    virtual void DoClick(DomNodeId dom_node_id,
                         base::OnceCallback<void(bool)> cb) = 0;
    virtual void DoPaste(DomNodeId dom_node_id,
                         FieldValue text,
                         base::OnceCallback<void(bool)> cb) = 0;
    virtual void DoSelect(DomNodeId dom_node_id,
                          FieldValue value,
                          base::OnceCallback<void(bool)> cb) = 0;
  };

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
  void GetElementSelector(DomNodeId dom_node_id,
                          base::OnceCallback<void(Selector)> cb);
  void GetMatchingElements(
      Selector element_selector,
      base::OnceCallback<void(const std::vector<DomNodeId>&)> cb);
  void DoClick(DomNodeId dom_node_id, base::OnceCallback<void(bool)> cb);
  void DoPaste(DomNodeId dom_node_id,
               FieldValue text,
               base::OnceCallback<void(bool)> cb);
  void DoSelect(DomNodeId dom_node_id,
                FieldValue value,
                base::OnceCallback<void(bool)> cb);

  // mojom::RecordReplayDriver:
  void OnClick(DomNodeId dom_node_id, Selector element_selector) override;
  void OnSelectChanged(DomNodeId dom_node_id,
                       Selector element_selector,
                       FieldValue text) override;
  void OnTextChange(DomNodeId dom_node_id,
                    Selector element_selector,
                    FieldValue text) override;

  void set_record_replay_agent_for_test(TestRecordReplayAgent* agent) {
    test_autofill_agent_ = agent;
  }

 private:
  const mojo::AssociatedRemote<mojom::RecordReplayAgent>& GetAgent();

  const raw_ref<RecordReplayClient> client_;
  const raw_ref<content::RenderFrameHost> rfh_;
  raw_ptr<TestRecordReplayAgent> test_autofill_agent_ = nullptr;
  mojo::AssociatedReceiver<mojom::RecordReplayDriver> receiver_{this};
  mojo::AssociatedRemote<mojom::RecordReplayAgent> agent_;
};

}  // namespace record_replay

#endif  // CHROME_BROWSER_RECORD_REPLAY_RECORD_REPLAY_DRIVER_H_
