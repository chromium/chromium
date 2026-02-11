// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_RECORD_REPLAY_RECORD_REPLAY_MANAGER_H_
#define CHROME_BROWSER_RECORD_REPLAY_RECORD_REPLAY_MANAGER_H_

#include <optional>
#include <string>

#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/types/pass_key.h"
#include "chrome/browser/record_replay/recorder.h"

namespace record_replay {

class ElementId;
class RecordReplayClient;
class RecordReplayDriver;

// Coordinates the recording and replay.
//
// Owned by RecordReplayClient.
class RecordReplayManager {
 public:
  enum class State { kIdle, kRecording, kReplaying };

  explicit RecordReplayManager(RecordReplayClient* client);
  RecordReplayManager(const RecordReplayManager&) = delete;
  RecordReplayManager& operator=(const RecordReplayManager&) = delete;
  ~RecordReplayManager();

  State state() const;

  // Starts or stops a recording.
  void StartRecording();
  void StopRecording();

  // Events that need to be recorded.
  void OnClick(RecordReplayDriver& driver,
               const ElementId& element_id,
               const std::string& element_selector,
               base::PassKey<RecordReplayDriver> pass_key);
  void OnSelectChanged(RecordReplayDriver& driver,
                       const ElementId& element_id,
                       const std::string& element_selector,
                       const std::string& value,
                       base::PassKey<RecordReplayDriver> pass_key);
  void OnTextChange(RecordReplayDriver& driver,
                    const ElementId& element_id,
                    const std::string& element_selector,
                    const std::string& text,
                    base::PassKey<RecordReplayDriver> pass_key);

  // Starts or stops the replay of the recording for the currently active page,
  // if one exists.
  void StartReplay();
  void StopReplay();

  base::WeakPtr<RecordReplayManager> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  void ReportToUser(std::string_view message);

  raw_ref<RecordReplayClient> client_;
  std::optional<Recorder> recorder_;
  base::WeakPtrFactory<RecordReplayManager> weak_ptr_factory_{this};
};

}  // namespace record_replay

#endif  // CHROME_BROWSER_RECORD_REPLAY_RECORD_REPLAY_MANAGER_H_
