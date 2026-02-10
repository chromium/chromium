// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/record_replay/record_replay_manager.h"

#include <string>

#include "base/barrier_callback.h"
#include "base/types/pass_key.h"
#include "chrome/browser/record_replay/element_id.h"
#include "chrome/browser/record_replay/record_replay_client.h"
#include "chrome/browser/record_replay/record_replay_driver.h"
#include "chrome/browser/record_replay/record_replay_driver_factory.h"

namespace record_replay {

RecordReplayManager::RecordReplayManager(RecordReplayClient* client)
    : client_(*client) {}

RecordReplayManager::~RecordReplayManager() = default;

RecordReplayManager::State RecordReplayManager::state() const {
  // TODO(b/476101114): Implement.
  return State::kIdle;
}

void RecordReplayManager::StartRecording() {
  // TODO(b/476101114): Start new recording.
  client_->GetDriverFactory().SetRecordForFutureDrivers(true);
  client_->GetDriverFactory().ForEachDriver(
      [](RecordReplayDriver& driver) { driver.StartRecording(); });
}

void RecordReplayManager::StopRecording() {
  // TODO(b/476101114): Stop and save ongoing recording.
  client_->GetDriverFactory().SetRecordForFutureDrivers(false);
  client_->GetDriverFactory().ForEachDriver(
      [](RecordReplayDriver& driver) { driver.StopRecording(); });
}

void RecordReplayManager::OnClick(RecordReplayDriver& driver,
                                  const ElementId& element_id,
                                  const std::string& element_selector,
                                  base::PassKey<RecordReplayDriver> pass_key) {
  // TODO(b/476101114): Implement.
}

void RecordReplayManager::OnSelectChanged(
    RecordReplayDriver& driver,
    const ElementId& element_id,
    const std::string& element_selector,
    const std::string& value,
    base::PassKey<RecordReplayDriver> pass_key) {
  // TODO(b/476101114): Implement.
}

void RecordReplayManager::OnTextChange(
    RecordReplayDriver& driver,
    const ElementId& element_id,
    const std::string& element_selector,
    const std::string& text,
    base::PassKey<RecordReplayDriver> pass_key) {
  // TODO(b/476101114): Implement.
}

void RecordReplayManager::StartReplay() {
  // TODO(b/476101114): Implement.
}

void RecordReplayManager::StopReplay() {
  // TODO(b/476101114): Implement.
}

void RecordReplayManager::ReportToUser(std::string_view message) {
  client_->ReportToUser(message);
}

}  // namespace record_replay
