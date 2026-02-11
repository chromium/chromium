// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/record_replay/record_replay_manager.h"

#include <optional>
#include <string>

#include "base/barrier_callback.h"
#include "base/containers/extend.h"
#include "base/containers/to_vector.h"
#include "base/functional/callback.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "base/types/optional_ref.h"
#include "base/types/pass_key.h"
#include "chrome/browser/record_replay/element_id.h"
#include "chrome/browser/record_replay/record_replay_client.h"
#include "chrome/browser/record_replay/record_replay_driver.h"
#include "chrome/browser/record_replay/record_replay_driver_factory.h"
#include "chrome/browser/record_replay/recorder.h"
#include "chrome/browser/record_replay/recording.pb.h"
#include "chrome/browser/record_replay/recording_data_manager.h"
#include "chrome/browser/record_replay/replayer.h"

namespace record_replay {

RecordReplayManager::RecordReplayManager(RecordReplayClient* client)
    : client_(*client) {}

RecordReplayManager::~RecordReplayManager() = default;

RecordReplayManager::State RecordReplayManager::state() const {
  if (replayer_) {
    return State::kReplaying;
  }
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

void RecordReplayManager::GetMatchingRecording(
    base::OnceCallback<void(std::optional<Recording>)> cb) {
  RecordingDataManager* rdm = client_->GetRecordingDataManager();
  if (!rdm) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(cb), std::nullopt));
    return;
  }
  base::optional_ref<const Recording> recording =
      rdm->GetRecording(client_->GetPrimaryMainFrameUrl().spec());
  if (!recording || recording->actions().empty()) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(cb), std::nullopt));
    return;
  }

  const std::string element_selector = recording->actions(0).element_selector();
  GetMatchingElements(
      element_selector,
      base::BindOnce(
          [](Recording recording, std::vector<ElementId> matches) {
            return matches.size() == 1 ? std::optional(std::move(recording))
                                       : std::nullopt;
          },
          *recording)
          .Then(std::move(cb)));
}

void RecordReplayManager::StartReplay() {
  if (replayer_) {
    return;
  }
  GetMatchingRecording(base::BindOnce(
      [](base::WeakPtr<RecordReplayManager> self,
         std::optional<Recording> recording) {
        if (!self || !recording) {
          return;
        }
        self->ReportToUser("Starting replay");
        self->replayer_.emplace(
            self.get(), *std::move(recording),
            base::BindOnce(&RecordReplayManager::StopReplay, self));
        self->replayer_->Run();
      },
      GetWeakPtr()));
}

void RecordReplayManager::StopReplay() {
  replayer_.reset();
}

void RecordReplayManager::GetMatchingElements(
    const std::string& element_selector,
    base::OnceCallback<void(std::vector<ElementId>)> cb) {
  std::vector<RecordReplayDriver*> drivers =
      client_->GetDriverFactory().GetActiveDrivers();

  base::RepeatingCallback<void(std::vector<ElementId>)> bcb =
      base::BarrierCallback<std::vector<ElementId>>(
          drivers.size(),
          base::BindOnce([](std::vector<std::vector<ElementId>> elements_vecs) {
            std::vector<ElementId> all_elements;
            for (std::vector<ElementId>& elements : elements_vecs) {
              base::Extend(all_elements, std::move(elements));
            }
            return all_elements;
          }).Then(std::move(cb)));

  for (RecordReplayDriver* driver : drivers) {
    driver->GetMatchingElements(
        element_selector,
        base::BindOnce(
            [](blink::LocalFrameToken frame_token,
               const std::vector<int64_t>& dom_node_ids) {
              return base::ToVector(dom_node_ids, [&](int64_t dom_node_id) {
                return ElementId{frame_token, dom_node_id};
              });
            },
            driver->GetFrameToken())
            .Then(bcb));
  }
}

void RecordReplayManager::ReportToUser(std::string_view message) {
  client_->ReportToUser(message);
}

}  // namespace record_replay
