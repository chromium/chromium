// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant;

import android.annotation.SuppressLint;
import android.content.pm.ActivityInfo;

import org.chromium.chrome.test.ChromeActivityTestRule;

class AutofillAssistantScreenOrientationHelper<T extends ChromeActivityTestRule> {
    private int mOriginalRequestedScreenOrientation;
    private final T mTestRule;

    AutofillAssistantScreenOrientationHelper(T testRule) {
        mTestRule = testRule;
    }

    @SuppressLint("SourceLockedOrientationActivity")
    public void setPortraitOrientation() {
        assert mTestRule.getActivity() != null;
        mOriginalRequestedScreenOrientation = mTestRule.getActivity().getRequestedOrientation();
        mTestRule.getActivity().setRequestedOrientation(ActivityInfo.SCREEN_ORIENTATION_PORTRAIT);
    }

    public void restoreOrientation() {
        assert mTestRule.getActivity() != null;
        mTestRule.getActivity().setRequestedOrientation(mOriginalRequestedScreenOrientation);
    }
}
