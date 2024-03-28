// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/mahi/mahi_ui_controller.h"

#include "base/logging.h"

namespace ash {

namespace {

// Returns true if `status` indicates an error.
bool HasError(chromeos::MahiResponseStatus status) {
  return status != chromeos::MahiResponseStatus::kSuccess;
}

}  // namespace

MahiUiController::MahiUiController() = default;

MahiUiController::~MahiUiController() = default;

void MahiUiController::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void MahiUiController::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void MahiUiController::NavigateToSummaryOutlinesSection() {
  for (auto& observer : observers_) {
    observer.OnNavigatedToSummaryOutlinesSection();
  }
}

void MahiUiController::NotifyRefreshAvailabilityChanged(bool available) {
  for (auto& observer : observers_) {
    observer.OnRefreshAvailabilityChanged(available);
  }
}

void MahiUiController::RefreshContents() {
  NavigateToSummaryOutlinesSection();

  for (auto& observer : observers_) {
    observer.OnContentsRefreshInitiated();
  }
}

void MahiUiController::SendQuestion(const std::u16string& question,
                                    bool current_panel_content) {
  for (auto& observer : observers_) {
    observer.OnQuestionPosted(question);
  }

  chromeos::MahiManager::Get()->AnswerQuestion(
      question, current_panel_content,
      base::BindOnce(&MahiUiController::OnAnswerLoaded,
                     weak_ptr_factory_.GetWeakPtr()));
}

void MahiUiController::UpdateSummaryAndOutlines() {
  chromeos::MahiManager::Get()->GetSummary(base::BindOnce(
      &MahiUiController::OnSummaryLoaded, weak_ptr_factory_.GetWeakPtr()));
  chromeos::MahiManager::Get()->GetOutlines(base::BindOnce(
      &MahiUiController::OnOutlinesLoaded, weak_ptr_factory_.GetWeakPtr()));
}

void MahiUiController::HandleErrorStatus(chromeos::MahiResponseStatus status) {
  CHECK(HasError(status));

  for (auto& observer : observers_) {
    observer.OnError(status);
  }
}

void MahiUiController::OnAnswerLoaded(std::optional<std::u16string> answer,
                                      chromeos::MahiResponseStatus status) {
  if (HasError(status)) {
    HandleErrorStatus(status);
    return;
  }

  // TODO(b/331302199): Handle the case that `answer` is `std::nullopt` in a
  // better way.
  if (!answer) {
    LOG(ERROR) << "Received an empty Mahi answer";
  }

  for (auto& observer : observers_) {
    observer.OnAnswerLoaded(answer.value_or(std::u16string()));
  }
}

void MahiUiController::OnOutlinesLoaded(
    std::vector<chromeos::MahiOutline> outlines,
    chromeos::MahiResponseStatus status) {
  if (HasError(status)) {
    HandleErrorStatus(status);
    return;
  }

  for (auto& observer : observers_) {
    observer.OnOutlinesLoaded(outlines);
  }
}

void MahiUiController::OnSummaryLoaded(std::u16string summary_text,
                                       chromeos::MahiResponseStatus status) {
  if (HasError(status)) {
    HandleErrorStatus(status);
    return;
  }

  for (auto& observer : observers_) {
    observer.OnSummaryLoaded(summary_text);
  }
}

}  // namespace ash
