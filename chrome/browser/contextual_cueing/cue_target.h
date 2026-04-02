// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTEXTUAL_CUEING_CUE_TARGET_H_
#define CHROME_BROWSER_CONTEXTUAL_CUEING_CUE_TARGET_H_

#include <string>
#include <variant>

namespace contextual_cueing {

// Identifier for features that show cues. Each feature that implements
// CueTarget should have a value in this enum.
enum class CueTargetType { kGlic = 0, kMaxValue = kGlic };

// Glic-specific click data.
struct GlicCueActionData {
  // Optional prompt to be filled in to glic upon opening.
  std::string prompt;
  // Whether to automatically send the prompt upon opening.
  bool auto_submit = false;
  // TODO(crbug.com/498987803): Possibly include tab sharing data
};

using CueActionData = std::variant<std::monostate, GlicCueActionData>;

// Interface representing a feature for which cues can be shown.
class CueTarget {
 public:
  virtual ~CueTarget() = default;

  // Whether the user is eligible for this feature's cues.
  virtual bool IsEligible() const = 0;

  // Called when the user clicks the cue's action button.
  virtual void OnClick(CueActionData data) = 0;
};

}  // namespace contextual_cueing

#endif  // CHROME_BROWSER_CONTEXTUAL_CUEING_CUE_TARGET_H_
