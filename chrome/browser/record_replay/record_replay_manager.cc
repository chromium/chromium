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
#include "base/i18n/time_formatting.h"
#include "base/strings/utf_string_conversions.h"
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
#include "chrome/common/record_replay/aliases.h"
#include "components/autofill/core/browser/data_model/addresses/autofill_profile.h"
#include "components/autofill/core/browser/data_model/payments/credit_card.h"
#include "third_party/abseil-cpp/absl/functional/overload.h"

namespace record_replay {

RecordReplayManager::RecordReplayManager(RecordReplayClient* client)
    : client_(*client) {
  if (auto* autofill_client = client_->GetAutofillClient()) {
    autofill_observation_.Observe(autofill_client);
  }
}

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
  ReportToUser("Starting recording");
  base::Time now = base::Time::Now();
  GURL url = client_->GetPrimaryMainFrameUrl();
  recorder_.emplace(url, now);
  recorder_->SetName(std::string(url.host()) + " - " +
                     base::UTF16ToUTF8(base::LocalizedTimeFormatWithPattern(
                         now, "yyyy-MM-dd")));

  // TODO(crbug.com/485828286): Capture a screenshot of the `WebContents` and
  // encode it as a JPEG before passing it to the recorder.
  recorder_->SetScreenshot({});

  client_->GetDriverFactory().SetRecordForFutureDrivers(true);
  client_->GetDriverFactory().ForEachDriver(
      [](RecordReplayDriver& driver) { driver.StartRecording(); });
}

std::optional<Recording> RecordReplayManager::StopRecording() {
  std::optional<Recording> recording;
  if (recorder_) {
    recording = std::move(recorder_->recording());
  }

  recorder_.reset();
  client_->GetDriverFactory().SetRecordForFutureDrivers(false);
  client_->GetDriverFactory().ForEachDriver(
      [](RecordReplayDriver& driver) { driver.StopRecording(); });

  return recording;
}

void RecordReplayManager::OnClick(RecordReplayDriver& driver,
                                  const ElementId& element_id,
                                  Selector element_selector,
                                  base::PassKey<RecordReplayDriver> pass_key) {
  if (!recorder_) {
    return;
  }
  recorder_->AddClick(std::move(element_selector));
}

void RecordReplayManager::OnSelectChanged(
    RecordReplayDriver& driver,
    const ElementId& element_id,
    Selector element_selector,
    FieldValue value,
    base::PassKey<RecordReplayDriver> pass_key) {
  if (!recorder_) {
    return;
  }
  recorder_->AddSelectChange(std::move(element_selector), std::move(value));
}

void RecordReplayManager::OnTextChange(
    RecordReplayDriver& driver,
    const ElementId& element_id,
    Selector element_selector,
    FieldValue text,
    base::PassKey<RecordReplayDriver> pass_key) {
  if (!recorder_) {
    return;
  }
  recorder_->AddTextChange(std::move(element_selector), std::move(text));
}

void RecordReplayManager::OnFillOrPreviewForm(
    autofill::AutofillManager& manager,
    autofill::FormGlobalId form_id,
    autofill::mojom::ActionPersistence action_persistence,
    const base::flat_set<autofill::FieldGlobalId>& filled_field_ids,
    const autofill::FillingPayload& filling_payload) {
  if (action_persistence != autofill::mojom::ActionPersistence ::kFill) {
    return;
  }
  if (filled_field_ids.empty()) {
    return;
  }
  struct Specifics {
    Recording::Action::AutofillSpecifics::Type type;
    std::string guid;
  };
  std::optional<Specifics> specifics = std::visit(
      absl::Overload{
          [](const autofill::AutofillProfile* ap) -> std::optional<Specifics> {
            return Specifics{
                Recording_Action_AutofillSpecifics_Type_AUTOFILL_PROFILE,
                ap->guid()};
          },
          [](const autofill::CreditCard* cc) -> std::optional<Specifics> {
            // TODO(b/483386299): If `cc` is a virtual credit card, is the GUID
            // meaningful?
            return Specifics{
                Recording_Action_AutofillSpecifics_Type_CREDIT_CARD,
                cc->guid()};
          },
          [](const autofill::EntityInstance* ei) -> std::optional<Specifics> {
            return Specifics{
                Recording_Action_AutofillSpecifics_Type_ENTITY_INSTANCE,
                *ei->guid()};
          },
          [](const autofill::VerifiedProfile*) -> std::optional<Specifics> {
            LOG(ERROR) << "VerifiedProfile is not supported";
            return std::nullopt;
          },
          [](const autofill::OtpFillData*) -> std::optional<Specifics> {
            LOG(ERROR) << "OtpFillData is not supported";
            return std::nullopt;
          },
      },
      filling_payload);
  if (!specifics) {
    return;
  }
  autofill::FieldGlobalId field_id = *filled_field_ids.begin();
  blink::LocalFrameToken token = blink::LocalFrameToken(*field_id.frame_token);
  DomNodeId dom_node_id = DomNodeId(*field_id.renderer_id);
  if (auto* driver = client_->GetDriverFactory().GetDriver(token)) {
    driver->GetElementSelector(
        dom_node_id,
        base::BindOnce(
            [](base::WeakPtr<RecordReplayManager> self, Specifics specifics,
               Selector element_selector) {
              if (!self || !self->recorder_ || element_selector->empty()) {
                return;
              }
              self->recorder_->AddAutofill(std::move(element_selector),
                                           specifics.type,
                                           std::move(specifics.guid));
            },
            GetWeakPtr(), *std::move(specifics)));
  }
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

  GetMatchingElements(
      Selector(recording->actions(0).element_selector()),
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
    Selector element_selector,
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
               const std::vector<DomNodeId>& dom_node_ids) {
              return base::ToVector(dom_node_ids, [&](DomNodeId dom_node_id) {
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
