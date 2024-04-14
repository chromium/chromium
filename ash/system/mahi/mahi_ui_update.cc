// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/mahi/mahi_ui_update.h"

#include <variant>

#include "base/check_op.h"
#include "chromeos/components/mahi/public/cpp/mahi_manager.h"

namespace ash {

// MahiQuestionParams ----------------------------------------------------------

MahiQuestionParams::MahiQuestionParams(const std::u16string& question,
                                       bool current_panel_content)
    : question(question), current_panel_content(current_panel_content) {}

MahiQuestionParams::~MahiQuestionParams() = default;

// MahiUiError -----------------------------------------------------------------

MahiUiError::MahiUiError(chromeos::MahiResponseStatus status,
                         VisibilityState origin_state)
    : status(status), origin_state(origin_state) {
  // `chromeos::MahiResponseStatus::kLowQuota` is a warning not an error.
  CHECK_NE(status, chromeos::MahiResponseStatus::kLowQuota);

  CHECK_NE(status, chromeos::MahiResponseStatus::kSuccess);
}

MahiUiError::~MahiUiError() = default;

// MahiUiUpdate ----------------------------------------------------------------

MahiUiUpdate::MahiUiUpdate(MahiUiUpdateType type)
    : type_(type), payload_(std::nullopt) {
  CheckTypeMatchesPayload();
}

MahiUiUpdate::MahiUiUpdate(MahiUiUpdateType type, bool payload)
    : type_(type), payload_(payload) {
  CheckTypeMatchesPayload();
}

MahiUiUpdate::MahiUiUpdate(MahiUiUpdateType type, const MahiUiError& payload)
    : type_(type), payload_(payload) {
  CheckTypeMatchesPayload();
}

MahiUiUpdate::MahiUiUpdate(MahiUiUpdateType type,
                           const MahiQuestionParams& payload)
    : type_(type), payload_(payload) {
  CheckTypeMatchesPayload();
}

MahiUiUpdate::MahiUiUpdate(MahiUiUpdateType type, const std::u16string& payload)
    : type_(type), payload_(payload) {
  CheckTypeMatchesPayload();
}

MahiUiUpdate::MahiUiUpdate(MahiUiUpdateType type,
                           const std::vector<chromeos::MahiOutline>& payload)
    : type_(type), payload_(payload) {
  CheckTypeMatchesPayload();
}

MahiUiUpdate::~MahiUiUpdate() = default;

const std::u16string& MahiUiUpdate::GetAnswer() const {
  CHECK_EQ(type_, MahiUiUpdateType::kAnswerLoaded);
  return std::get<std::reference_wrapper<const std::u16string>>(*payload_);
}

const MahiUiError& MahiUiUpdate::GetError() const {
  CHECK_EQ(type_, MahiUiUpdateType::kErrorReceived);
  return std::get<std::reference_wrapper<const MahiUiError>>(*payload_);
}

const std::vector<chromeos::MahiOutline>& MahiUiUpdate::GetOutlines() const {
  CHECK_EQ(type_, MahiUiUpdateType::kOutlinesLoaded);
  return std::get<
      std::reference_wrapper<const std::vector<chromeos::MahiOutline>>>(
      *payload_);
}

const std::u16string& MahiUiUpdate::GetQuestion() const {
  CHECK_EQ(type_, MahiUiUpdateType::kQuestionPosted);
  return std::get<std::reference_wrapper<const std::u16string>>(*payload_);
}

const MahiQuestionParams& MahiUiUpdate::GetReAskQuestionParams() const {
  CHECK_EQ(type_, MahiUiUpdateType::kQuestionReAsked);
  return std::get<std::reference_wrapper<const MahiQuestionParams>>(*payload_);
}

bool MahiUiUpdate::GetRefreshAvailability() const {
  CHECK_EQ(type_, MahiUiUpdateType::kRefreshAvailabilityUpdated);
  return std::get<bool>(*payload_);
}

const std::u16string& MahiUiUpdate::GetSummary() const {
  CHECK_EQ(type_, MahiUiUpdateType::kSummaryLoaded);
  return std::get<std::reference_wrapper<const std::u16string>>(*payload_);
}

void MahiUiUpdate::CheckTypeMatchesPayload() {
  switch (type_) {
    case MahiUiUpdateType::kAnswerLoaded:
      CHECK(payload_.has_value());
      CHECK(
          std::holds_alternative<std::reference_wrapper<const std::u16string>>(
              *payload_));
      break;
    case MahiUiUpdateType::kContentsRefreshInitiated:
      CHECK(!payload_.has_value());
      break;
    case MahiUiUpdateType::kErrorReceived:
      CHECK(payload_.has_value());
      CHECK(std::holds_alternative<std::reference_wrapper<const MahiUiError>>(
          *payload_));
      break;
    case MahiUiUpdateType::kOutlinesLoaded:
      CHECK(payload_.has_value());
      CHECK(std::holds_alternative<
            std::reference_wrapper<const std::vector<chromeos::MahiOutline>>>(
          *payload_));
      break;
    case MahiUiUpdateType::kQuestionAndAnswerViewNavigated:
      CHECK(!payload_.has_value());
      break;
    case MahiUiUpdateType::kQuestionPosted:
      CHECK(payload_.has_value());
      CHECK(
          std::holds_alternative<std::reference_wrapper<const std::u16string>>(
              *payload_));
      break;
    case MahiUiUpdateType::kQuestionReAsked:
      CHECK(payload_.has_value());
      CHECK(std::holds_alternative<
            std::reference_wrapper<const MahiQuestionParams>>(*payload_));
      break;
    case MahiUiUpdateType::kRefreshAvailabilityUpdated:
      CHECK(payload_.has_value());
      CHECK(std::holds_alternative<bool>(*payload_));
      break;
    case MahiUiUpdateType::kSummaryAndOutlinesSectionNavigated:
      CHECK(!payload_.has_value());
      break;
    case MahiUiUpdateType::kSummaryLoaded:
      CHECK(payload_.has_value());
      CHECK(
          std::holds_alternative<std::reference_wrapper<const std::u16string>>(
              *payload_));
      break;
    case MahiUiUpdateType::kSummaryAndOutlinesReloaded:
      CHECK(!payload_.has_value());
      break;
  }
}

}  // namespace ash
