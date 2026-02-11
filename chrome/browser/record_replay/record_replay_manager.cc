// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/record_replay/record_replay_manager.h"

#include <optional>
#include <string>

#include "base/barrier_callback.h"
#include "base/types/pass_key.h"
#include "chrome/browser/record_replay/element_id.h"
#include "chrome/browser/record_replay/record_replay_client.h"
#include "chrome/browser/record_replay/record_replay_driver.h"
#include "chrome/browser/record_replay/record_replay_driver_factory.h"
#include "chrome/browser/record_replay/recorder.h"
#include "chrome/browser/record_replay/recording_data_manager.h"

namespace record_replay {

RecordReplayManager::RecordReplayManager(RecordReplayClient* client)
    : client_(*client) {}

RecordReplayManager::~RecordReplayManager() = default;

RecordReplayManager::State RecordReplayManager::state() const {
  // TODO(b/476101114): Implement.
  if (recorder_) {
    return State::kRecording;
  }
  return State::kIdle;
}

void RecordReplayManager::StartRecording() {
  if (recorder_) {
    ReportToUser("Finished recording");
    if (RecordingDataManager* rdm = client_->GetRecordingDataManager()) {
      rdm->AddRecording(recorder_->recording());
    }
  }

  ReportToUser("Starting recording");
  recorder_.emplace(client_->GetPrimaryMainFrameUrl(), base::Time::Now());
  client_->GetDriverFactory().SetRecordForFutureDrivers(true);
  client_->GetDriverFactory().ForEachDriver(
      [](RecordReplayDriver& driver) { driver.StartRecording(); });
}

void RecordReplayManager::StopRecording() {
  if (recorder_) {
    ReportToUser("Finished recording");
    if (RecordingDataManager* rdm = client_->GetRecordingDataManager()) {
      rdm->AddRecording(recorder_->recording());
    }
  }

  recorder_.reset();
  client_->GetDriverFactory().SetRecordForFutureDrivers(false);
  client_->GetDriverFactory().ForEachDriver(
      [](RecordReplayDriver& driver) { driver.StopRecording(); });
}

void RecordReplayManager::OnClick(RecordReplayDriver& driver,
                                  const ElementId& element_id,
                                  const std::string& element_selector,
                                  base::PassKey<RecordReplayDriver> pass_key) {
  if (!recorder_) {
    return;
  }
  recorder_->AddClick(element_selector);
}

void RecordReplayManager::OnSelectChanged(
    RecordReplayDriver& driver,
    const ElementId& element_id,
    const std::string& element_selector,
    const std::string& value,
    base::PassKey<RecordReplayDriver> pass_key) {
  if (!recorder_) {
    return;
  }
  recorder_->AddSelectChange(element_selector, value);
}

void RecordReplayManager::OnTextChange(
    RecordReplayDriver& driver,
    const ElementId& element_id,
    const std::string& element_selector,
    const std::string& text,
    base::PassKey<RecordReplayDriver> pass_key) {
  if (!recorder_) {
    return;
  }
  recorder_->AddTextChange(element_selector, text);
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
