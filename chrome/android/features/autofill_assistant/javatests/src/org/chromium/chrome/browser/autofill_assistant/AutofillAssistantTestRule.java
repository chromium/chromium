// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant;

import android.annotation.SuppressLint;
import android.content.pm.ActivityInfo;

import org.junit.rules.TestRule;
import org.junit.runner.Description;
import org.junit.runners.model.Statement;

import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.components.autofill_assistant.AutofillAssistantPreferencesUtil;

abstract class AutofillAssistantTestRule<T extends ChromeActivityTestRule> implements TestRule {
    private final T mTestRule;

    private int mOriginalRequestedScreenOrientation;

    AutofillAssistantTestRule(T testRule) {
        mTestRule = testRule;
    }

    T getTestRule() {
        return mTestRule;
    }

    abstract void startActivity();
    abstract void cleanupAfterTest();

    @Override
    public Statement apply(final Statement base, Description description) {
        return new Statement() {
            @Override
            public void evaluate() throws Throwable {
                AutofillAssistantPreferencesUtil.setInitialPreferences(true);
                startActivity();
                setPortraitOrientation();
                mTestRule.getActivity()
                        .getRootUiCoordinatorForTesting()
                        .getScrimCoordinator()
                        .disableAnimationForTesting(true);
                try {
                    base.evaluate();
                } finally {
                    restoreOrientation();
                }
                cleanupAfterTest();
            }
        };
    }

    @SuppressLint("SourceLockedOrientationActivity")
    private void setPortraitOrientation() {
        assert mTestRule.getActivity() != null;
        mOriginalRequestedScreenOrientation = mTestRule.getActivity().getRequestedOrientation();
        mTestRule.getActivity().setRequestedOrientation(ActivityInfo.SCREEN_ORIENTATION_PORTRAIT);
    }

    private void restoreOrientation() {
        assert mTestRule.getActivity() != null;
        mTestRule.getActivity().setRequestedOrientation(mOriginalRequestedScreenOrientation);
    }
}
