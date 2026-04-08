// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTEXTUAL_CUEING_CONTEXTUAL_CUEING_SERVICE_H_
#define CHROME_BROWSER_CONTEXTUAL_CUEING_CONTEXTUAL_CUEING_SERVICE_H_

#include <memory>

#include "chrome/browser/contextual_cueing/cue_target.h"
#include "components/keyed_service/core/keyed_service.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"

namespace contextual_cueing {

class ContextualCueingService : public KeyedService {
 public:
  ContextualCueingService();
  ~ContextualCueingService() override;

  // Register a cue type. Feature code provides a CueTarget for reporting the
  // feature's cue eligibility and handling clicks. Calling this function for a
  // CueTargetType that was already registered will destroy the previous target.
  // Once registered, cue types are never unregistered -- features may prevent
  // cues by returning false from IsEligible.
  void RegisterCueTarget(CueTargetType type, std::unique_ptr<CueTarget> target);

  // Called when the user clicks the cue action button. This should only be
  // called for registered target features.
  void OnClick(CueTargetType type, CueActionData data);

  // Called when the user dismisses the cue. This should only be called for
  // registered target features.
  void OnDismiss(CueTargetType type);

  // Look up a registered CueTarget.
  CueTarget* GetTarget(CueTargetType type);

 private:
  absl::flat_hash_map<CueTargetType, std::unique_ptr<CueTarget>> cue_targets_;
};

}  // namespace contextual_cueing

#endif  // CHROME_BROWSER_CONTEXTUAL_CUEING_CONTEXTUAL_CUEING_SERVICE_H_
