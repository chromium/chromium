// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTEXTUAL_CUEING_MOCK_CONTEXTUAL_CUEING_SERVICE_H_
#define CHROME_BROWSER_CONTEXTUAL_CUEING_MOCK_CONTEXTUAL_CUEING_SERVICE_H_

#include "chrome/browser/contextual_cueing/contextual_cueing_service.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace contextual_cueing {

class MockContextualCueingService : public ContextualCueingService {
 public:
  MockContextualCueingService();
  ~MockContextualCueingService() override;

  MOCK_METHOD(void, ReportPageLoad, ());
  MOCK_METHOD(void,
              OnNudgeActivity,
              (content::WebContents*,
               base::TimeTicks,
               tabs::GlicNudgeActivity));
  MOCK_METHOD(void, CueingNudgeShown, (const GURL&));
  MOCK_METHOD(void, CueingNudgeDismissed, ());
  MOCK_METHOD(void, CueingNudgeClicked, ());
  MOCK_METHOD(NudgeDecision, CanShowNudge, (const GURL&));
  MOCK_METHOD(void,
              PrepareToFetchContextualGlicZeroStateSuggestions,
              (content::WebContents*));
  MOCK_METHOD(void,
              GetContextualGlicZeroStateSuggestions,
              (content::WebContents*, bool, GlicSuggestionsCallback));
};

}  // namespace contextual_cueing

#endif  // CHROME_BROWSER_CONTEXTUAL_CUEING_MOCK_CONTEXTUAL_CUEING_SERVICE_H_
