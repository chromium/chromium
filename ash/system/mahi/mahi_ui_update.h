// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_MAHI_MAHI_UI_UPDATE_H_
#define ASH_SYSTEM_MAHI_MAHI_UI_UPDATE_H_

#include <functional>
#include <optional>
#include <string>
#include <variant>
#include <vector>

#include "ash/ash_export.h"

namespace chromeos {
struct MahiOutline;
enum class MahiResponseStatus;
}  // namespace chromeos

namespace ash {

enum class VisibilityState {
  // The state that shows the view displaying errors from user actions.
  // NOTE: Not all errors should be displayed in this state.
  kError,

  // The state that shows the view displaying questions and answers.
  kQuestionAndAnswer,

  // The state that shows the view displaying summary and outlines.
  kSummaryAndOutlines,
};

enum class MahiUiUpdateType {
  // An answer is loaded successfully.
  kAnswerLoaded,

  // A request to refresh the panel contents is initiated.
  kContentsRefreshInitiated,

  // An error is received.
  kErrorReceived,

  // Outlines are loaded successfully.
  kOutlinesLoaded,

  // The question and answer view is requested to show.
  kQuestionAndAnswerViewNavigated,

  // A question is posted by user.
  kQuestionPosted,

  // A question is re-asked by user.
  kQuestionReAsked,

  // The content refresh availability changes.
  kRefreshAvailabilityUpdated,

  // The summary and outlines section is requested to show.
  kSummaryAndOutlinesSectionNavigated,

  // A summary is loaded with a success.
  kSummaryLoaded,

  // The summary and outlines are requested to reload.
  kSummaryAndOutlinesReloaded,
};

// Contains the params required to send a question to the Mahi backend.
struct MahiQuestionParams {
  MahiQuestionParams(const std::u16string& question,
                     bool current_panel_content);
  MahiQuestionParams(const MahiQuestionParams&) = delete;
  MahiQuestionParams& operator=(const MahiQuestionParams&) = delete;
  ~MahiQuestionParams();

  const std::u16string question;

  // Determines if the `question` is regarding the current content displayed on
  // the panel.
  const bool current_panel_content;
};

// Describes a Mahi UI error, including its origin and status.
struct MahiUiError {
  MahiUiError(chromeos::MahiResponseStatus status,
              VisibilityState origin_state);
  MahiUiError(const MahiUiError&) = delete;
  MahiUiError& operator=(const MahiUiError&) = delete;
  ~MahiUiError();

  // The error status. NOTE: `status` should not be
  // `chromeos::MahiResponseStatus::kSuccess` or
  // `chromeos::MahiResponseStatus::kLowQuota`.
  const chromeos::MahiResponseStatus status;

  // Indicates the `VisibilityState` where `status` comes from.
  const VisibilityState origin_state;
};

// Indicates a change that triggers a visible update on the Mahi UI.
class ASH_EXPORT MahiUiUpdate {
 public:
  explicit MahiUiUpdate(MahiUiUpdateType type);
  MahiUiUpdate(MahiUiUpdateType type, bool payload);

  // NOTE: `MahiUiUpdate` caches the const reference to `payload`, not a copy.
  // The class user has the duty to ensure the original `payload` object
  // outlives the `MahiUiUpdate` instance.
  MahiUiUpdate(MahiUiUpdateType type, const MahiUiError& payload);
  MahiUiUpdate(MahiUiUpdateType type, const MahiQuestionParams& payload);
  MahiUiUpdate(MahiUiUpdateType type, const std::u16string& payload);
  MahiUiUpdate(MahiUiUpdateType type,
               const std::vector<chromeos::MahiOutline>& payload);

  MahiUiUpdate(const MahiUiUpdate&) = delete;
  MahiUiUpdate& operator=(const MahiUiUpdate&) = delete;
  ~MahiUiUpdate();

  // Returns the answer from `payload`.
  // NOTE: This function should be called only if `type` is `kAnswerLoaded`.
  const std::u16string& GetAnswer() const;

  // Returns the error from `payload`.
  // NOTE: This function should be called only if `type` is `kErrorReceived`.
  const MahiUiError& GetError() const;

  // Returns the outlines from `payload`.
  // NOTE: This function should be called only if `type` is `kOutlinesLoaded`.
  const std::vector<chromeos::MahiOutline>& GetOutlines() const;

  // Returns the question from `payload`.
  // NOTE: This function should be called only if `type` is `kQuestionPosted`.
  const std::u16string& GetQuestion() const;

  // Returns the params required to re-ask a question.
  // NOTE: This function should be called only if `type` is `kQuestionReAsked`.
  const MahiQuestionParams& GetReAskQuestionParams() const;

  // Returns the refresh availability from `payload`.
  // NOTE: This function should be called only if `type` is
  // `kRefreshAvailabilityUpdated`.
  bool GetRefreshAvailability() const;

  // Returns the summary from `payload`.
  // NOTE: This function should be called only if `type` is `kSummaryLoaded`.
  const std::u16string& GetSummary() const;

  MahiUiUpdateType type() const { return type_; }

 private:
  // Check that `type_` matches the actual type of `payload_`.
  void CheckTypeMatchesPayload();

  const MahiUiUpdateType type_;

  // Provides more details on the update. Its actual data depends on `type`:
  // For `kAnswerLoaded`, `payload` is an answer;
  // For `kContentsRefreshInitiated`, `payload` is `std::nullopt`;
  // For `kErrorReceived`, `payload` is an error;
  // For `kOutlinesLoaded`, `payload` is an array of outlines;
  // For `kQuestionAndAnswerViewNavigated`, `payload` is `std::nullopt`;
  // For `kQuestionPosted`, `payload` is a question;
  // For `kQuestionReAsked`, `payload` is a question params struct;
  // For `kRefreshAvailabilityUpdated`, `payload` is a boolean;
  // For `kSummaryAndOutlinesSectionNavigated`, `payload` is `std::nullopt`;
  // For `kSummaryLoaded`, `payload` is a summary;
  // For `kSummaryAndOutlinesReloaded`, `payload` is `std::nullopt`.
  using PayloadType = std::variant<
      std::reference_wrapper<const std::u16string>,
      std::reference_wrapper<const MahiQuestionParams>,
      std::reference_wrapper<const MahiUiError>,
      std::reference_wrapper<const std::vector<chromeos::MahiOutline>>,
      bool>;
  const std::optional<PayloadType> payload_;
};

}  // namespace ash

#endif  // ASH_SYSTEM_MAHI_MAHI_UI_UPDATE_H_
