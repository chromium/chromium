// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_MAHI_MAHI_UI_UPDATE_H_
#define ASH_SYSTEM_MAHI_MAHI_UI_UPDATE_H_

#include <functional>
#include <optional>
#include <variant>
#include <vector>

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

  // A question is posted by user.
  kQuestionPosted,

  // The content refresh availability changes.
  kRefreshAvailabilityUpdated,

  // The summary and outlines section is requested to show.
  kSummaryAndOutlinesSectionNavigated,

  // A summary is loaded with a success.
  kSummaryLoaded,
};

// Indicates a change that triggers a visible update on the Mahi UI.
class MahiUiUpdate {
 public:
  explicit MahiUiUpdate(MahiUiUpdateType type);
  MahiUiUpdate(MahiUiUpdateType type, chromeos::MahiResponseStatus payload);
  MahiUiUpdate(MahiUiUpdateType type, bool payload);

  // NOTE: `MahiUiUpdate` caches the const reference to `payload`, not a copy.
  // The class user has the duty to ensure the original `payload` object
  // outlives the `MahiUiUpdate` instance.
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
  chromeos::MahiResponseStatus GetError() const;

  // Returns the outlines from `payload`.
  // NOTE: This function should be called only if `type` is `kOutlinesLoaded`.
  const std::vector<chromeos::MahiOutline>& GetOutlines() const;

  // Returns the question from `payload`.
  // NOTE: This function should be called only if `type` is `kQuestionPosted`.
  const std::u16string& GetQuestion() const;

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
  // For `kQuestionPosted`, `payload` is a question;
  // For `kRefreshAvailabilityUpdated`, `payload` is a boolean;
  // For `kSummaryAndOutlinesSectionNavigated`, `payload` is `std::nullopt`;
  // For `kSummaryLoaded`, `payload` is a summary.
  using PayloadType = std::variant<
      std::reference_wrapper<const std::u16string>,
      chromeos::MahiResponseStatus,
      std::reference_wrapper<const std::vector<chromeos::MahiOutline>>,
      bool>;
  const std::optional<PayloadType> payload_;
};

}  // namespace ash

#endif  // ASH_SYSTEM_MAHI_MAHI_UI_UPDATE_H_
