// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTEXTUAL_CUEING_CONTEXTUAL_CUEING_SERVICE_H_
#define CHROME_BROWSER_CONTEXTUAL_CUEING_CONTEXTUAL_CUEING_SERVICE_H_

#include "chrome/browser/contextual_cueing/cue_target.h"
#include "components/keyed_service/core/keyed_service.h"

namespace contextual_cueing {

class ContextualCueingService : public KeyedService {
 public:
  ContextualCueingService();
  ~ContextualCueingService() override;

  // Called when the user clicks the cue action button.
  void OnClick(CueTargetType type);

  // Called when the user dismisses the cue.
  void OnDismiss(CueTargetType type);
};

}  // namespace contextual_cueing

#endif  // CHROME_BROWSER_CONTEXTUAL_CUEING_CONTEXTUAL_CUEING_SERVICE_H_
