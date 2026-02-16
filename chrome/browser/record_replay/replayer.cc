// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/record_replay/replayer.h"

#include <string>
#include <vector>

#include "base/check.h"
#include "base/check_op.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "base/notreached.h"
#include "base/strings/stringprintf.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "chrome/browser/record_replay/element_id.h"
#include "chrome/browser/record_replay/record_replay_client.h"
#include "chrome/browser/record_replay/record_replay_driver.h"
#include "chrome/browser/record_replay/record_replay_driver_factory.h"
#include "chrome/browser/record_replay/record_replay_manager.h"
#include "chrome/browser/record_replay/recording.pb.h"

namespace record_replay {

namespace {
constexpr int kNumMaxRetries = 2;
constexpr base::TimeDelta kRetryDelay = base::Seconds(3);
}  // namespace

Replayer::Replayer(RecordReplayManager* owner,
                   Recording recording,
                   base::OnceClosure on_finish)
    : owner_(*owner),
      recording_(std::move(recording)),
      on_finish_(std::move(on_finish)) {
  CHECK(!on_finish_.is_null());
}

Replayer::~Replayer() = default;

int Replayer::num_actions() const {
  return recording_.actions_size();
}

const Recording::Action& Replayer::action(int index) const {
  CHECK_LT(index, recording_.actions_size());
  return recording_.actions(index);
}

void Replayer::Run() {
  CHECK(!timer_.IsRunning());
  CHECK(!on_finish_.is_null());
  Loop(0);
}

void Replayer::Loop(int index) {
  if (index < 0) {
    owner_->ReportToUser("Action failed");
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, std::move(on_finish_));
    return;
  }
  if (index >= num_actions()) {
    owner_->ReportToUser("Replay finished");
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, std::move(on_finish_));
    return;
  }
  DoAction(index, kNumMaxRetries);
}

void Replayer::DoAction(int index, int num_max_retries) {
  const base::TimeDelta delta = num_max_retries == kNumMaxRetries
                                    ? base::Microseconds(action(index).delta())
                                    : kRetryDelay;
  SuccessCallback cb = base::BindOnce(
      [](base::WeakPtr<Replayer> self, int index, int num_max_retries,
         bool success) {
        if (!self) {
          return;
        }
        if (success) {
          self->Loop(index + 1);
          return;
        }
        if (num_max_retries >= 1) {
          self->DoAction(index, num_max_retries - 1);
          return;
        }
        self->Loop(-1);
      },
      GetWeakPtr(), index, num_max_retries);
  timer_.Start(
      FROM_HERE, delta,
      base::BindOnce(
          [](Replayer* self, int index, SuccessCallback cb) {
            const Recording::Action& action = self->action(index);
            if (action.has_autofill_specifics()) {
              self->ReplayAutofillAction(action.element_selector(),
                                         action.autofill_specifics(),
                                         std::move(cb));
            } else if (action.has_click_specifics()) {
              self->ReplayClickAction(action.element_selector(), std::move(cb));
            } else if (action.has_select_specifics()) {
              self->ReplaySelectChangeAction(action.element_selector(),
                                             action.select_specifics().value(),
                                             std::move(cb));
            } else if (action.has_text_specifics()) {
              self->ReplayTextChangeAction(action.element_selector(),
                                           action.text_specifics().value(),
                                           std::move(cb));
            } else {
              LOG(ERROR) << "Invalid action";
            }
          },
          base::Unretained(this), index, std::move(cb)));
}

void Replayer::ReplayAutofillAction(
    const std::string& element_selector,
    const Recording::Action::AutofillSpecifics& specifics,
    SuccessCallback cb) {
  // TODO(b/483386299): An autofill is currently recorded twice: once as
  // autofill operation, once as the text-change operations. If we replay
  // autofills, we should of course not replay the text-change operations.
  LOG(ERROR) << __func__ << " isn't supported yet";
  std::move(cb).Run(true);
}

void Replayer::ReplayClickAction(const std::string& element_selector,
                                 SuccessCallback cb) {
  GetUniqueMatchingElementsAndDo(
      element_selector,
      base::BindOnce(
          [](RecordReplayDriver& driver, ElementId match, SuccessCallback cb) {
            driver.DoClick(match.dom_node_id(), std::move(cb));
          }),
      std::move(cb));
}

void Replayer::ReplaySelectChangeAction(const std::string& element_selector,
                                        std::string value,
                                        SuccessCallback cb) {
  GetUniqueMatchingElementsAndDo(
      element_selector,
      base::BindOnce(
          [](std::string value, RecordReplayDriver& driver, ElementId match,
             SuccessCallback cb) {
            driver.DoSelect(match.dom_node_id(), value, std::move(cb));
          },
          std::move(value)),
      std::move(cb));
}

void Replayer::ReplayTextChangeAction(const std::string& element_selector,
                                      std::string text,
                                      SuccessCallback cb) {
  GetUniqueMatchingElementsAndDo(
      element_selector,
      base::BindOnce(
          [](std::string text, RecordReplayDriver& driver, ElementId match,
             SuccessCallback cb) {
            driver.DoPaste(match.dom_node_id(), text, std::move(cb));
          },
          std::move(text)),
      std::move(cb));
}

void Replayer::GetUniqueMatchingElementsAndDo(
    const std::string& element_selector,
    base::OnceCallback<void(RecordReplayDriver&, ElementId, SuccessCallback)>
        action_cb,
    SuccessCallback result_cb) {
  owner_->GetMatchingElements(
      element_selector,
      base::BindOnce(
          [](base::WeakPtr<Replayer> self,
             base::OnceCallback<void(RecordReplayDriver&, ElementId,
                                     SuccessCallback)> action_cb,
             SuccessCallback result_cb, std::vector<ElementId> matches) {
            if (!self) {
              std::move(result_cb).Run(false);
              return;
            }
            if (matches.size() != 1) {
              self->owner_->ReportToUser(base::StringPrintf(
                  "Selector is not unique (%zu). Skipping action.",
                  matches.size()));
              std::move(result_cb).Run(false);
              return;
            }
            ElementId match = matches.front();
            RecordReplayDriver* driver =
                self->owner_->client().GetDriverFactory().GetDriver(
                    match.frame_token());
            if (!driver) {
              std::move(result_cb).Run(false);
              return;
            }
            std::move(action_cb).Run(*driver, match, std::move(result_cb));
          },
          GetWeakPtr(), std::move(action_cb), std::move(result_cb)));
}

}  // namespace record_replay
