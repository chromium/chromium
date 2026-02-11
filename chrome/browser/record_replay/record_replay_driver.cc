// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/record_replay/record_replay_driver.h"

#include "base/functional/callback.h"
#include "chrome/browser/record_replay/element_id.h"
#include "chrome/browser/record_replay/record_replay_client.h"
#include "chrome/browser/record_replay/record_replay_manager.h"
#include "content/public/browser/render_frame_host.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/blink/public/common/tokens/tokens.h"

namespace record_replay {

RecordReplayDriver::RecordReplayDriver(
    content::RenderFrameHost* render_frame_host,
    RecordReplayClient& client)
    : client_(client), rfh_(*render_frame_host) {}

RecordReplayDriver::~RecordReplayDriver() = default;

void RecordReplayDriver::BindPendingReceiver(
    mojo::PendingAssociatedReceiver<mojom::RecordReplayDriver>
        pending_receiver) {
  receiver_.Bind(std::move(pending_receiver));
}

bool RecordReplayDriver::IsActive() const {
  return rfh_->IsActive();
}

const blink::LocalFrameToken& RecordReplayDriver::GetFrameToken() const {
  return rfh_->GetFrameToken();
}

void RecordReplayDriver::StartRecording() {
  // TODO(b/476101114): Implement.
  if (test_autofill_agent_) {
    test_autofill_agent_->StartRecording();
  }
}

void RecordReplayDriver::StopRecording() {
  // TODO(b/476101114): Implement.
  if (test_autofill_agent_) {
    test_autofill_agent_->StopRecording();
  }
}

void RecordReplayDriver::GetElementSelector(
    int64_t dom_node_id,
    base::OnceCallback<void(const std::string&)> cb) {
  // TODO(b/476101114): Implement.
  if (test_autofill_agent_) {
    test_autofill_agent_->GetElementSelector(dom_node_id, std::move(cb));
  }
}

void RecordReplayDriver::GetMatchingElements(
    const std::string& element_selector,
    base::OnceCallback<void(const std::vector<int64_t>&)> cb) {
  // TODO(b/476101114): Implement.
  if (test_autofill_agent_) {
    test_autofill_agent_->GetMatchingElements(element_selector, std::move(cb));
  }
}

void RecordReplayDriver::DoClick(int64_t dom_node_id,
                                 base::OnceCallback<void(bool)> cb) {
  // TODO(b/476101114): Implement.
  if (test_autofill_agent_) {
    test_autofill_agent_->DoClick(dom_node_id, std::move(cb));
  }
}

void RecordReplayDriver::DoPaste(int64_t dom_node_id,
                                 const std::string& text,
                                 base::OnceCallback<void(bool)> cb) {
  // TODO(b/476101114): Implement.
  if (test_autofill_agent_) {
    test_autofill_agent_->DoPaste(dom_node_id, text, std::move(cb));
  }
}

void RecordReplayDriver::DoSelect(int64_t dom_node_id,
                                  const std::string& value,
                                  base::OnceCallback<void(bool)> cb) {
  // TODO(b/476101114): Implement.
  if (test_autofill_agent_) {
    test_autofill_agent_->DoSelect(dom_node_id, value, std::move(cb));
  }
}

void RecordReplayDriver::OnClick(int64_t dom_node_id,
                                 const std::string& element_selector) {
  client_->GetManager().OnClick(*this, {GetFrameToken(), dom_node_id},
                                element_selector,
                                /*pass_key=*/{});
}

void RecordReplayDriver::OnSelectChanged(int64_t dom_node_id,
                                         const std::string& element_selector,
                                         const std::string& value) {
  client_->GetManager().OnSelectChanged(*this, {GetFrameToken(), dom_node_id},
                                        element_selector, value,
                                        /*pass_key=*/{});
}

void RecordReplayDriver::OnTextChange(int64_t dom_node_id,
                                      const std::string& element_selector,
                                      const std::string& text) {
  client_->GetManager().OnTextChange(*this, {GetFrameToken(), dom_node_id},
                                     element_selector, text,
                                     /*pass_key=*/{});
}

}  // namespace record_replay
