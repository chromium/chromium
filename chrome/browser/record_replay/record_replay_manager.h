// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_RECORD_REPLAY_RECORD_REPLAY_MANAGER_H_
#define CHROME_BROWSER_RECORD_REPLAY_RECORD_REPLAY_MANAGER_H_

#include <optional>
#include <string>
#include <vector>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/types/pass_key.h"
#include "chrome/browser/record_replay/recorder.h"
#include "chrome/browser/record_replay/replayer.h"
#include "chrome/common/record_replay/aliases.h"
#include "components/autofill/core/browser/foundations/autofill_manager.h"
#include "components/autofill/core/browser/foundations/scoped_autofill_managers_observation.h"

namespace record_replay {

class ElementId;
class RecordReplayClient;
class RecordReplayDriver;

// Coordinates the recording and replay.
//
// Owned by RecordReplayClient.
class RecordReplayManager : public autofill::AutofillManager::Observer {
 public:
  enum class State { kIdle, kRecording, kReplaying };

  explicit RecordReplayManager(RecordReplayClient* client);
  RecordReplayManager(const RecordReplayManager&) = delete;
  RecordReplayManager& operator=(const RecordReplayManager&) = delete;
  ~RecordReplayManager() override;

  RecordReplayClient& client() { return *client_; }

  State state() const;

  // Starts or stops a recording.
  void StartRecording();
  std::optional<Recording> StopRecording();

  // Events that need to be recorded.
  void OnClick(RecordReplayDriver& driver,
               const ElementId& element_id,
               Selector element_selector,
               base::PassKey<RecordReplayDriver> pass_key);
  void OnSelectChanged(RecordReplayDriver& driver,
                       const ElementId& element_id,
                       Selector element_selector,
                       FieldValue value,
                       base::PassKey<RecordReplayDriver> pass_key);
  void OnTextChange(RecordReplayDriver& driver,
                    const ElementId& element_id,
                    Selector element_selector,
                    FieldValue text,
                    base::PassKey<RecordReplayDriver> pass_key);

  // Starts or stops the replay of the recording for the currently active page,
  // if one exists.
  void StartReplay();
  void StopReplay();

  // Retrieves the recording for the last committed URL, if there is one, and
  // passes it to `cb`.
  void GetMatchingRecording(
      base::OnceCallback<void(std::optional<Recording>)> cb);

  // Retrieves all elements in all active frames that match `element_selector`.
  void GetMatchingElements(Selector element_selector,
                           base::OnceCallback<void(std::vector<ElementId>)> cb);

  void ReportToUser(std::string_view message);

  base::WeakPtr<RecordReplayManager> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  // Fired for fills made by Autofill, but not by Password Manager!
  void OnFillOrPreviewForm(
      autofill::AutofillManager& manager,
      autofill::FormGlobalId form_id,
      autofill::mojom::ActionPersistence action_persistence,
      const base::flat_set<autofill::FieldGlobalId>& filled_field_ids,
      const autofill::FillingPayload& filling_payload) override;

  raw_ref<RecordReplayClient> client_;
  std::optional<Recorder> recorder_;
  std::optional<Replayer> replayer_;
  autofill::ScopedAutofillManagersObservation autofill_observation_{this};
  base::WeakPtrFactory<RecordReplayManager> weak_ptr_factory_{this};
};

}  // namespace record_replay

#endif  // CHROME_BROWSER_RECORD_REPLAY_RECORD_REPLAY_MANAGER_H_
