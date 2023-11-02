// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant;

import android.app.Activity;

import org.chromium.chrome.browser.feedback.HelpAndFeedbackLauncherImpl;
import org.chromium.chrome.browser.feedback.ScreenshotMode;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.autofill_assistant.AssistantFeedbackUtil;
import org.chromium.content_public.browser.WebContents;

/**
 * Implementation of {@link AssistantFeedbackUtil} for Chrome.
 */
public class AssistantFeedbackUtilChrome implements AssistantFeedbackUtil {
    private static final String FEEDBACK_CATEGORY_TAG =
            "com.android.chrome.USER_INITIATED_FEEDBACK_REPORT_AUTOFILL_ASSISTANT";

    @Override
    public void showFeedback(Activity activity, WebContents webContents,
            @ScreenshotMode int screenshotMode, String debugContext) {
        HelpAndFeedbackLauncherImpl.getInstance().showFeedback(activity,
                Profile.fromWebContents(webContents), webContents.getVisibleUrl().getSpec(),
                FEEDBACK_CATEGORY_TAG, screenshotMode, debugContext);
    }
}
