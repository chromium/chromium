// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/mahi/mahi_ui_update.h"

#include "base/check_op.h"
#include "chromeos/components/mahi/public/cpp/mahi_manager.h"

namespace ash {

// MahiUiUpdate ----------------------------------------------------------------

MahiUiUpdate::MahiUiUpdate(MahiUiUpdateType type)
    : type_(type), payload_(std::nullopt) {
  CheckTypeMatchesPayload();
}

MahiUiUpdate::MahiUiUpdate(MahiUiUpdateType type,
                           chromeos::MahiResponseStatus payload)
    : type_(type), payload_(payload) {
  CheckTypeMatchesPayload();
}

MahiUiUpdate::MahiUiUpdate(MahiUiUpdateType type, bool payload)
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

chromeos::MahiResponseStatus MahiUiUpdate::GetError() const {
  CHECK_EQ(type_, MahiUiUpdateType::kErrorReceived);
  return std::get<chromeos::MahiResponseStatus>(*payload_);
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
      CHECK(std::holds_alternative<chromeos::MahiResponseStatus>(*payload_));
      CHECK_NE(std::get<chromeos::MahiResponseStatus>(*payload_),
               chromeos::MahiResponseStatus::kSuccess);
      break;
    case MahiUiUpdateType::kOutlinesLoaded:
      CHECK(payload_.has_value());
      CHECK(std::holds_alternative<
            std::reference_wrapper<const std::vector<chromeos::MahiOutline>>>(
          *payload_));
      break;
    case MahiUiUpdateType::kQuestionPosted:
      CHECK(payload_.has_value());
      CHECK(
          std::holds_alternative<std::reference_wrapper<const std::u16string>>(
              *payload_));
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
  }
}

}  // namespace ash
