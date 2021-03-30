// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import android.app.Activity;
import android.content.Intent;
import android.os.Bundle;

import androidx.appcompat.app.AppCompatActivity;

import org.chromium.base.ApplicationStatus;
import org.chromium.base.IntentUtils;
import org.chromium.chrome.browser.app.ChromeActivity;
import org.chromium.chrome.browser.share.ShareHelper;
import org.chromium.components.browser_ui.widget.MenuOrKeyboardActionController;

/**
 * {@code ChromeActivityAccessor} is the base class for share options, which
 * are activities that are shown in the share chooser. Activities subclassing
 * ChromeAccessorActivity override handleAction.
 */
public abstract class ChromeAccessorActivity extends AppCompatActivity {
    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        try {
            Intent intent = getIntent();
            if (intent == null) return;
            if (!Intent.ACTION_SEND.equals(intent.getAction())) return;
            if (!IntentUtils.safeHasExtra(intent, ShareHelper.EXTRA_TASK_ID)) return;

            ChromeActivity triggeringActivity = getTriggeringActivity();
            if (triggeringActivity == null) return;

            handleAction(/* triggeringActivity= */ triggeringActivity,
                    /* menuOrKeyboardActionController= */ triggeringActivity);
        } finally {
            finish();
        }
    }

    /**
     * Returns the ChromeActivity that called the share intent picker.
     */
    private ChromeActivity getTriggeringActivity() {
        int triggeringTaskId =
                IntentUtils.safeGetIntExtra(getIntent(), ShareHelper.EXTRA_TASK_ID, 0);
        for (Activity activity : ApplicationStatus.getRunningActivities()) {
            if (activity.getTaskId() == triggeringTaskId && activity instanceof ChromeActivity) {
                return (ChromeActivity) activity;
            }
        }
        return null;
    }

    /**
     * Completes the share action.
     *
     * Override this activity to implement desired share functionality.  This activity
     * will be destroyed immediately after this method is called.
     *
     * @param triggeringActivity The {@link Activity} that triggered the share.
     * @param menuOrKeyboardActionController Handles menu or keyboard actions.
     */
    protected abstract void handleAction(Activity triggeringActivity,
            MenuOrKeyboardActionController menuOrKeyboardActionController);
}
