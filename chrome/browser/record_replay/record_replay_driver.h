// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_RECORD_REPLAY_RECORD_REPLAY_DRIVER_H_
#define CHROME_BROWSER_RECORD_REPLAY_RECORD_REPLAY_DRIVER_H_

#include <vector>

#include "base/functional/callback_forward.h"
#include "base/types/pass_key.h"
#include "chrome/common/record_replay/aliases.h"
#include "third_party/blink/public/common/tokens/tokens.h"

namespace record_replay {

// The browser-side endpoint for communication with the renderer
// implementation of the interface (`RecordReplayAgent`).
class RecordReplayDriver {
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

  virtual ~RecordReplayDriver() = default;

  // Returns true if the driver's frame is active (i.e., neither
  // prerendered nor bfcached).
  virtual bool IsActive() const = 0;

  // Returns the unique identifier of the driver's frame.
  virtual const blink::LocalFrameToken& GetFrameToken() const = 0;

  // See mojom::RecordReplayAgent record_replay.mojom.
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

  virtual void set_record_replay_agent_for_test(
      TestRecordReplayAgent* agent) = 0;

 protected:
  base::PassKey<RecordReplayDriver> GetPassKey() {
    return base::PassKey<RecordReplayDriver>();
  }
};

}  // namespace record_replay

#endif  // CHROME_BROWSER_RECORD_REPLAY_RECORD_REPLAY_DRIVER_H_
