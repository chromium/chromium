// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTEXTUAL_CUEING_CUE_TARGET_H_
#define CHROME_BROWSER_CONTEXTUAL_CUEING_CUE_TARGET_H_

#include <string>
#include <variant>

#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "components/optimization_guide/proto/features/contextual_cueing.pb.h"
#include "components/tabs/public/tab_interface.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_set.h"
#include "ui/base/models/image_model.h"

namespace contextual_cueing {

// Identifier for features that show cues. Each feature that implements
// CueTarget should have a value in this enum.
enum class CueTargetType { kGlic = 0, kMaxValue = kGlic };
const char* GetName(CueTargetType type);

// Glic-specific click data.
struct GlicCueActionData {
  // Optional prompt to be filled in to glic upon opening.
  std::string prompt;
  // Tabs that should be used as context ("pinned") by glic.
  std::vector<tabs::TabHandle> tabs_to_share;

  GlicCueActionData();
  ~GlicCueActionData();
  GlicCueActionData(const GlicCueActionData&);
  GlicCueActionData(GlicCueActionData&&);
  GlicCueActionData& operator=(const GlicCueActionData&);
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

  // Icon to be shown in the anchored message.
  virtual ui::ImageModel GetAnchoredMessageIcon() const = 0;

  // Icon to be shown in the omnibox chip.
  virtual ui::ImageModel GetOmniboxChipIcon() const = 0;

  // Extract this feature's click data from a contextual cueing response.
  virtual CueActionData CueActionDataFromResponse(
      const optimization_guide::proto::ContextualCueingResponse& response)
      const = 0;

  virtual optimization_guide::proto::ContextualCueingSurface GetSurface()
      const = 0;
};

}  // namespace contextual_cueing

#endif  // CHROME_BROWSER_CONTEXTUAL_CUEING_CUE_TARGET_H_
