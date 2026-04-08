// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/record_replay/content_record_replay_driver.h"

#include "base/functional/callback.h"
#include "chrome/browser/record_replay/element_id.h"
#include "chrome/browser/record_replay/record_replay_client.h"
#include "chrome/browser/record_replay/record_replay_manager.h"
#include "chrome/common/record_replay/aliases.h"
#include "content/public/browser/render_frame_host.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/blink/public/common/tokens/tokens.h"

namespace record_replay {

ContentRecordReplayDriver::ContentRecordReplayDriver(
    content::RenderFrameHost* render_frame_host,
    RecordReplayClient& client)
    : client_(client), rfh_(*render_frame_host) {}

ContentRecordReplayDriver::~ContentRecordReplayDriver() = default;

void ContentRecordReplayDriver::BindPendingReceiver(
    mojo::PendingAssociatedReceiver<mojom::RecordReplayDriver>
        pending_receiver) {
  receiver_.Bind(std::move(pending_receiver));
}

const mojo::AssociatedRemote<mojom::RecordReplayAgent>&
ContentRecordReplayDriver::GetAgent() {
  if (!agent_) {
    rfh_->GetRemoteAssociatedInterfaces()->GetInterface(&agent_);
  }
  return agent_;
}

bool ContentRecordReplayDriver::IsActive() const {
  return rfh_->IsActive();
}

const blink::LocalFrameToken& ContentRecordReplayDriver::GetFrameToken() const {
  return rfh_->GetFrameToken();
}

void ContentRecordReplayDriver::StartRecording() {
  if (test_autofill_agent_) {
    test_autofill_agent_->StartRecording();
    return;
  }
  GetAgent()->StartRecording();
}

void ContentRecordReplayDriver::StopRecording() {
  if (test_autofill_agent_) {
    test_autofill_agent_->StopRecording();
    return;
  }
  GetAgent()->StopRecording();
}

void ContentRecordReplayDriver::GetElementSelector(
    DomNodeId dom_node_id,
    base::OnceCallback<void(Selector)> cb) {
  if (test_autofill_agent_) {
    test_autofill_agent_->GetElementSelector(dom_node_id, std::move(cb));
    return;
  }
  GetAgent()->GetElementSelector(dom_node_id, std::move(cb));
}

void ContentRecordReplayDriver::GetMatchingElements(
    Selector element_selector,
    base::OnceCallback<void(const std::vector<DomNodeId>&)> cb) {
  if (test_autofill_agent_) {
    test_autofill_agent_->GetMatchingElements(std::move(element_selector),
                                              std::move(cb));
    return;
  }
  GetAgent()->GetMatchingElements(std::move(element_selector), std::move(cb));
}

void ContentRecordReplayDriver::DoClick(DomNodeId dom_node_id,
                                        base::OnceCallback<void(bool)> cb) {
  if (test_autofill_agent_) {
    test_autofill_agent_->DoClick(dom_node_id, std::move(cb));
    return;
  }
  GetAgent()->DoClick(dom_node_id, std::move(cb));
}

void ContentRecordReplayDriver::DoPaste(DomNodeId dom_node_id,
                                        FieldValue text,
                                        base::OnceCallback<void(bool)> cb) {
  if (test_autofill_agent_) {
    test_autofill_agent_->DoPaste(dom_node_id, std::move(text), std::move(cb));
    return;
  }
  GetAgent()->DoPaste(dom_node_id, text, std::move(cb));
}

void ContentRecordReplayDriver::DoSelect(DomNodeId dom_node_id,
                                         FieldValue value,
                                         base::OnceCallback<void(bool)> cb) {
  if (test_autofill_agent_) {
    test_autofill_agent_->DoSelect(dom_node_id, std::move(value),
                                   std::move(cb));
    return;
  }
  GetAgent()->DoSelect(dom_node_id, value, std::move(cb));
}

void ContentRecordReplayDriver::set_record_replay_agent_for_test(
    TestRecordReplayAgent* agent) {
  test_autofill_agent_ = agent;
}

void ContentRecordReplayDriver::OnClick(DomNodeId dom_node_id,
                                        Selector element_selector) {
  client_->GetManager().OnClick(*this, {GetFrameToken(), dom_node_id},
                                std::move(element_selector), GetPassKey());
}

void ContentRecordReplayDriver::OnSelectChanged(DomNodeId dom_node_id,
                                                Selector element_selector,
                                                FieldValue value) {
  client_->GetManager().OnSelectChanged(*this, {GetFrameToken(), dom_node_id},
                                        std::move(element_selector),
                                        std::move(value), GetPassKey());
}

void ContentRecordReplayDriver::OnTextChange(DomNodeId dom_node_id,
                                             Selector element_selector,
                                             FieldValue text) {
  client_->GetManager().OnTextChange(*this, {GetFrameToken(), dom_node_id},
                                     std::move(element_selector),
                                     std::move(text), GetPassKey());
}

}  // namespace record_replay
