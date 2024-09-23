// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.dragdrop;

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
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.multiwindow.MultiWindowUtils;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.ui.dragdrop.DragDropMetricUtils;
import org.chromium.ui.dragdrop.DragDropMetricUtils.DragDropType;
import org.chromium.ui.dragdrop.DragDropMetricUtils.UrlIntentSource;

/** A helper activity for routing Chrome tab and link drag & drop launcher intents. */
// TODO (crbug/331865433): Consider removing use of this trampoline activity.
public class DragAndDropLauncherActivity extends Activity {
    static final String ACTION_DRAG_DROP_VIEW = "org.chromium.chrome.browser.dragdrop.action.VIEW";
    static final String LAUNCHED_FROM_LINK_USER_ACTION = "MobileNewInstanceLaunchedFromDraggedLink";
    static final String LAUNCHED_FROM_TAB_USER_ACTION = "MobileNewInstanceLaunchedFromDraggedTab";

    private static final long DROP_TIMEOUT_MS = 5 * TimeUtils.MILLISECONDS_PER_MINUTE;
    private static Long sIntentCreationTimestampMs;
    private static Long sDropTimeoutForTesting;

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

        recordLaunchMetrics(intent);

        // Launch the intent in an existing Chrome window, referenced by the EXTRA_WINDOW_ID intent
        // extra, if required.
        if (intent.hasExtra(IntentHandler.EXTRA_WINDOW_ID)) {
            int windowId =
                    IntentUtils.safeGetIntExtra(
                            intent,
                            IntentHandler.EXTRA_WINDOW_ID,
                            MultiWindowUtils.INVALID_INSTANCE_ID);
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
     * @param windowId The window ID of the Chrome window in which the link will be opened,
     *     |MultiWindowUtils.INVALID_INSTANCE_ID| if there is no preference.
     * @param intentSrc An enum indicating whether the intent is created by link or tab.
     * @return The intent that will be used to create a new Chrome instance from a dragged link.
     */
    public static Intent getLinkLauncherIntent(
            Context context, String urlString, int windowId, @UrlIntentSource int intentSrc) {
        Intent intent =
                MultiWindowUtils.createNewWindowIntent(
                        context.getApplicationContext(),
                        windowId,
                        /* preferNew= */ true,
                        /* openAdjacently= */ false,
                        /* addTrustedIntentExtras= */ false);
        intent.setClass(context, DragAndDropLauncherActivity.class);
        intent.setAction(DragAndDropLauncherActivity.ACTION_DRAG_DROP_VIEW);
        intent.addCategory(Intent.CATEGORY_BROWSABLE);
        intent.setData(Uri.parse(urlString));
        intent.putExtra(IntentHandler.EXTRA_URL_DRAG_SOURCE, intentSrc);
        DragAndDropLauncherActivity.setIntentCreationTimestampMs(SystemClock.elapsedRealtime());
        return intent;
    }

    /**
     * Creates an intent from a tab dragged out of Chrome to move it to a new Chrome window.
     *
     * @param context The context used to retrieve the package name.
     * @param tab The dragged tab.
     * @param windowId The window ID of the Chrome window in which the tab will be moved,
     *     |MultiWindowUtils.INVALID_INSTANCE_ID| if the tab should be moved to a new window.
     * @return The intent that will be used to move a dragged tab to a new Chrome instance.
     */
    public static Intent getTabIntent(Context context, Tab tab, int windowId) {
        if (!MultiWindowUtils.isMultiInstanceApi31Enabled()) return null;
        Intent intent =
                MultiWindowUtils.createNewWindowIntent(
                        context.getApplicationContext(),
                        windowId,
                        /* preferNew= */ true,
                        /* openAdjacently= */ false,
                        /* addTrustedIntentExtras= */ false);
        intent.setClass(context, DragAndDropLauncherActivity.class);
        intent.setAction(DragAndDropLauncherActivity.ACTION_DRAG_DROP_VIEW);
        intent.putExtra(IntentHandler.EXTRA_URL_DRAG_SOURCE, UrlIntentSource.TAB_IN_STRIP);
        intent.putExtra(IntentHandler.EXTRA_DRAGGED_TAB_ID, tab.getId());
        intent.addCategory(Intent.CATEGORY_BROWSABLE);
        intent.setData(Uri.parse(tab.getUrl().getSpec()));
        DragAndDropLauncherActivity.setIntentCreationTimestampMs(SystemClock.elapsedRealtime());
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
        // Exit early if the original intent action isn't for viewing a dragged link/tab.
        assert ACTION_DRAG_DROP_VIEW.equals(intent.getAction()) : "The intent action is invalid.";

        // Exit early if the duration between the original intent creation and drop to launch the
        // activity exceeds the timeout.
        return getIntentCreationTimestampMs() != null
                && (SystemClock.elapsedRealtime() - getIntentCreationTimestampMs()
                        <= getDropTimeoutMs());
    }

    /**
     * Sets the ClipData intent creation timestamp when a Chrome link/tab drag starts.
     *
     * @param timestamp The intent creation timestamp in milliseconds.
     */
    static void setIntentCreationTimestampMs(Long timestamp) {
        sIntentCreationTimestampMs = timestamp;
    }

    /**
     * @return The dragged link/tab intent creation timestamp in milliseconds.
     */
    static Long getIntentCreationTimestampMs() {
        return sIntentCreationTimestampMs;
    }

    @VisibleForTesting
    static Long getDropTimeoutMs() {
        return sDropTimeoutForTesting == null ? DROP_TIMEOUT_MS : sDropTimeoutForTesting;
    }

    static void setDropTimeoutMsForTesting(Long timeout) {
        sDropTimeoutForTesting = timeout;
        ResettersForTesting.register(() -> sDropTimeoutForTesting = null);
    }

    @VisibleForTesting
    static @DragDropType int getDragDropTypeFromIntent(Intent intent) {
        switch (intent.getIntExtra(IntentHandler.EXTRA_URL_DRAG_SOURCE, UrlIntentSource.UNKNOWN)) {
            case UrlIntentSource.LINK:
                return DragDropType.LINK_TO_NEW_INSTANCE;
            case UrlIntentSource.TAB_IN_STRIP:
                return DragDropType.TAB_STRIP_TO_NEW_INSTANCE;
            default:
                return DragDropType.UNKNOWN_TO_NEW_INSTANCE;
        }
    }

    private static void recordLaunchMetrics(Intent intent) {
        @UrlIntentSource
        int intentSource =
                IntentUtils.safeGetIntExtra(
                        intent, IntentHandler.EXTRA_URL_DRAG_SOURCE, UrlIntentSource.UNKNOWN);
        if (intentSource == UrlIntentSource.LINK) {
            RecordUserAction.record(LAUNCHED_FROM_LINK_USER_ACTION);
        } else if (intentSource == UrlIntentSource.TAB_IN_STRIP) {
            RecordUserAction.record(LAUNCHED_FROM_TAB_USER_ACTION);
        }
        DragDropMetricUtils.recordTabDragDropType(getDragDropTypeFromIntent(intent));
    }
}
