// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.net.Uri;
import android.os.Bundle;
import android.os.SystemClock;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.IntentUtils;
import org.chromium.base.ResettersForTesting;
import org.chromium.base.TimeUtils;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.browser.multiwindow.MultiWindowUtils;

/**
 * A helper activity for routing Chrome link drag & drop launcher intents.
 */
public class DragAndDropLauncherActivity extends Activity {
    static final String ACTION_DRAG_DROP_VIEW = "org.chromium.chrome.browser.dragdrop.action.VIEW";
    static final String LAUNCHED_FROM_LINK_USER_ACTION = "MobileNewInstanceLaunchedFromDraggedLink";

    private static final long LINK_DROP_TIMEOUT_MS = 5 * TimeUtils.MILLISECONDS_PER_MINUTE;
    private static Long sLinkIntentCreationTimestampMs;
    private static Long sLinkDropTimeoutForTesting;

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        var intent = getIntent();
        if (!isIntentValid(intent)) {
            finish();
            return;
        }

        // Launch the intent in a new or existing ChromeTabbedActivity.
        intent.setClass(this, ChromeTabbedActivity.class);
        IntentUtils.addTrustedIntentExtras(intent);
        RecordUserAction.record(LAUNCHED_FROM_LINK_USER_ACTION);

        // Launch the intent in an existing Chrome window, referenced by the EXTRA_WINDOW_ID intent
        // extra, if required. This extra will be present when the maximum number of instances is
        // open, to determine which window to open the link in.
        if (intent.hasExtra(IntentHandler.EXTRA_WINDOW_ID)) {
            int windowId = IntentUtils.safeGetIntExtra(
                    intent, IntentHandler.EXTRA_WINDOW_ID, MultiWindowUtils.INVALID_INSTANCE_ID);
            MultiWindowUtils.launchIntentInInstance(intent, windowId);
        } else {
            startActivity(intent);
        }

        finish();
    }

    /**
     * Creates an intent from a link dragged out of Chrome to open a new Chrome window.
     *
     * @param context The context used to retrieve the package name.
     * @param urlString The link URL string.
     * @param windowId The window ID of the Chrome window in which the link will be opened, {@code
     *         null} if there is no preference.
     * @return The intent that will be used to create a new Chrome instance from a dragged link.
     */
    public static Intent getLinkLauncherIntent(
            Context context, String urlString, Integer windowId) {
        windowId =
                windowId == null ? MultiWindowUtils.getRunningInstanceIdForViewIntent() : windowId;
        Intent intent = MultiWindowUtils.createNewWindowIntent(context.getApplicationContext(),
                windowId, /*preferNew=*/true,
                /*openAdjacently=*/false, /*addTrustedIntentExtras=*/false);
        intent.setClass(context, DragAndDropLauncherActivity.class);
        intent.setAction(DragAndDropLauncherActivity.ACTION_DRAG_DROP_VIEW);
        intent.addCategory(Intent.CATEGORY_BROWSABLE);
        intent.setData(Uri.parse(urlString));
        DragAndDropLauncherActivity.setLinkIntentCreationTimestampMs(SystemClock.elapsedRealtime());
        return intent;
    }

    /**
     * Validates the intent before processing it.
     *
     * @param intent The incoming intent.
     * @return {@code true} if the intent is valid for processing, {@code false} otherwise.
     */
    @VisibleForTesting
    static boolean isIntentValid(Intent intent) {
        // Exit early if the original intent action isn't for viewing a dragged link.
        assert ACTION_DRAG_DROP_VIEW.equals(intent.getAction()) : "The intent action is invalid.";

        // Exit early if the duration between the original intent creation and drop to launch the
        // activity exceeds the timeout.
        return getLinkIntentCreationTimestampMs() != null
                && (SystemClock.elapsedRealtime() - getLinkIntentCreationTimestampMs()
                        <= getLinkDropTimeoutMs());
    }

    /**
     * Sets the ClipData intent creation timestamp when a Chrome link drag starts.
     *
     * @param timestamp The intent creation timestamp in milliseconds.
     */
    static void setLinkIntentCreationTimestampMs(Long timestamp) {
        sLinkIntentCreationTimestampMs = timestamp;
    }

    /**
     * @return The dragged link intent creation timestamp in milliseconds.
     */
    static Long getLinkIntentCreationTimestampMs() {
        return sLinkIntentCreationTimestampMs;
    }

    @VisibleForTesting
    static Long getLinkDropTimeoutMs() {
        return sLinkDropTimeoutForTesting == null ? LINK_DROP_TIMEOUT_MS
                                                  : sLinkDropTimeoutForTesting;
    }

    static void setLinkDropTimeoutMsForTesting(Long timeout) {
        sLinkDropTimeoutForTesting = timeout;
        ResettersForTesting.register(() -> sLinkDropTimeoutForTesting = null);
    }
}
