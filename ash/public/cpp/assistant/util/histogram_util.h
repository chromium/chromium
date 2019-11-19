// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_ASSISTANT_UTIL_HISTOGRAM_UTIL_H_
#define ASH_PUBLIC_CPP_ASSISTANT_UTIL_HISTOGRAM_UTIL_H_

#include "ash/public/cpp/ash_public_export.h"

namespace ash {

namespace assistant {
namespace metrics {

// Enumeration of possible results for a proactive suggestions server request.
// Note that this enum is used in UMA histograms so new values should only be
// appended to the end.
enum class ProactiveSuggestionsRequestResult {
  kError = 0,
  kSuccessWithContent = 1,
  kSuccessWithoutContent = 2,
  kMaxValue = kSuccessWithoutContent,
};

// Enumeration of possible attempt resolutions for showing a proactive
// suggestion to the user. Note that this enum is used in UMA histograms so new
// values should only be appended to the end.
enum class ProactiveSuggestionsShowAttempt {
  kSuccess = 0,
  kAbortedByDuplicateSuppression = 1,
  kMaxValue = kAbortedByDuplicateSuppression,
};

// Enumeration of possible results for having shown a proactive suggestion to
// the user. Note that this enum is used in UMA histograms so new values should
// only be appended to the end.
enum class ProactiveSuggestionsShowResult {
  kClick = 0,
  kCloseByContextChange = 1,
  kCloseByTimeout = 2,
  kCloseByUser = 3,
  kMaxValue = kCloseByUser,
};

// Records a |result| for a proactive suggestions server request in the
// specified content |category|. Note that |category| is an opaque int that is
// provided by the proactive suggestions server to represent the category of the
// associated content (e.g. news, shopping, etc.).
ASH_PUBLIC_EXPORT void RecordProactiveSuggestionsRequestResult(
    int category,
    ProactiveSuggestionsRequestResult result);

// Records an |attempt| to show a proactive suggestion to the user in the
// specified content |category|. Note that |category| is an opaque int that is
// provided by the proactive suggestions server to represent the category of the
// associated content (e.g. news, shopping, etc.).
ASH_PUBLIC_EXPORT void RecordProactiveSuggestionsShowAttempt(
    int category,
    ProactiveSuggestionsShowAttempt attempt);

// Records a |result| from having shown a proactive suggestion to the user in
// the specified content |category|. Note that |category| is an opaque int that
// is provided by the proactive suggestions server to represent the category of
// the associated content (e.g. news, shopping, etc.).
ASH_PUBLIC_EXPORT void RecordProactiveSuggestionsShowResult(
    int category,
    ProactiveSuggestionsShowResult result);

}  // namespace metrics
}  // namespace assistant
}  // namespace ash

#endif  // ASH_PUBLIC_CPP_ASSISTANT_UTIL_HISTOGRAM_UTIL_H_