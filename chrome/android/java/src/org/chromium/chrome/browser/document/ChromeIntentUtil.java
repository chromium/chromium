// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.document;

import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.provider.Browser;
import android.support.annotation.Nullable;

import org.chromium.base.ApplicationStatus;
import org.chromium.base.ContextUtils;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.customtabs.CustomTabActivity;

/**
 * Utilities to construct intents to Chrome.
 */
public class ChromeIntentUtil {
    public static final String TAG = "ChromeIntentUtil";

    /**
     * Creates an Intent that tells Chrome to bring an Activity for a particular Tab back to the
     * foreground.
     * @param tabId The id of the Tab to bring to the foreground.
     * @return Created Intent or null if this operation isn't possible.
     */
    @Nullable
    public static Intent createBringTabToFrontIntent(int tabId) {
        // Iterate through all {@link CustomTab}s and check whether the given tabId belongs to a
        // {@link CustomTab}. If so, return null as the client app's task cannot be foregrounded.
        for (Activity activity : ApplicationStatus.getRunningActivities()) {
            if (activity instanceof CustomTabActivity
                    && ((CustomTabActivity) activity).getActivityTab() != null
                    && tabId == ((CustomTabActivity) activity).getActivityTab().getId()) {
                return null;
            }
        }

        Context context = ContextUtils.getApplicationContext();
        Intent intent = new Intent(context, ChromeLauncherActivity.class);
        intent.putExtra(Browser.EXTRA_APPLICATION_ID, context.getPackageName());
        intent.putExtra(IntentHandler.TabOpenType.BRING_TAB_TO_FRONT_STRING, tabId);
        return intent;
    }
}
