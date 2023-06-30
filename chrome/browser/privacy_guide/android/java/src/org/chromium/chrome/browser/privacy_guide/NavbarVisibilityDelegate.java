// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.privacy_guide;

import android.view.View;

/**
 * A delegate class to compute the visibility of each button in Privacy Guide
 * {@link PrivacyGuideFragment}
 */
class NavbarVisibilityDelegate {
    private final int mTotalSteps;

    NavbarVisibilityDelegate(int totalSteps) {
        assert totalSteps >= 3 : "At least the Welcome, MSBB and Done cards are displayed";
        mTotalSteps = totalSteps;
    }

    int getStartButtonVisibility(int currentStepIdx) {
        return isFirstCard(currentStepIdx) ? View.VISIBLE : View.GONE;
    }

    int getNextButtonVisibility(int currentStepIdx) {
        return isCardBetweenFirstAndLast(currentStepIdx) && !isSecondToLastCard(currentStepIdx)
                ? View.VISIBLE
                : View.GONE;
    }

    int getBackButtonVisibility(int currentStepIdx) {
        return isCardBetweenFirstAndLast(currentStepIdx) ? View.VISIBLE : View.GONE;
    }

    int getFinishButtonVisibility(int currentStepIdx) {
        return isSecondToLastCard(currentStepIdx) ? View.VISIBLE : View.GONE;
    }

    int getDoneButtonVisibility(int currentStepIdx) {
        return isLastCard(currentStepIdx) ? View.VISIBLE : View.GONE;
    }

    int getProgressIndicatorVisibility(int currentStepIdx) {
        return isCardBetweenFirstAndLast(currentStepIdx) ? View.VISIBLE : View.GONE;
    }

    private boolean isFirstCard(int currentStepIdx) {
        return currentStepIdx == 0;
    }

    private boolean isCardBetweenFirstAndLast(int currentStepIdx) {
        return currentStepIdx > 0 && currentStepIdx < mTotalSteps - 1;
    }

    private boolean isSecondToLastCard(int currentStepIdx) {
        return currentStepIdx == mTotalSteps - 2;
    }

    private boolean isLastCard(int currentStepIdx) {
        return currentStepIdx == mTotalSteps - 1;
    }
}
