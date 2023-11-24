// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.screenshot_monitor;

import androidx.annotation.Nullable;

import org.chromium.base.UserData;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.ukm.UkmRecorder;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.WindowAndroid;

/** A {@link TabObserver} that also handles screenshot related events. */
public class ScreenshotTabObserver extends EmptyTabObserver implements UserData {
    // Enum for logging Screenshot UMA. These match the TabScreenshotAction enum in enums.xml, and
    // must not be changed.  New ones can be added if we also add them in enums.xml.
    public static final int SCREENSHOT_ACTION_NONE = 0;
    public static final int SCREENSHOT_ACTION_SHARE = 1;
    public static final int SCREENSHOT_ACTION_DOWNLOAD_IPH = 2;
    // If new actions are added, count must always be one higher than the last number.
    public static final int SCREENSHOT_ACTION_COUNT = 3;

    private static final Class<ScreenshotTabObserver> USER_DATA_KEY = ScreenshotTabObserver.class;
    private Runnable mOnReportCompleteForTesting;

    /**
     * Gets the existing observer if it exists, otherwise creates one.
     * @param tab The Tab for which to create the observer.
     * @return ScreenshotTabObserver to use, or null if the tab was null.
     */
    public static ScreenshotTabObserver from(Tab tab) {
        if (tab == null || !tab.isInitialized()) return null;
        // Get the current observer from the tab using UserData, if any.  If not, create a new
        // observer and put it into the UserData for the tab.
        ScreenshotTabObserver observer = get(tab);
        if (observer == null) {
            observer =
                    tab.getUserDataHost()
                            .setUserData(USER_DATA_KEY, new ScreenshotTabObserver(tab));
            tab.addObserver(observer);
        }
        return observer;
    }

    /**
     * Returns {@link ScreenshotTabObserver} object for a given {@link Tab}, or {@code null}
     * if there is no object available.
     */
    public static @Nullable ScreenshotTabObserver get(Tab tab) {
        if (tab == null || !tab.isInitialized()) return null;
        return tab.getUserDataHost().getUserData(USER_DATA_KEY);
    }

    /** Number of screenshots taken of the tab while on the same page */
    private int mScreenshotsTaken;

    /** Actions performed after a screenshot was taken. */
    private int mScreenshotAction;

    private Tab mTab;

    public ScreenshotTabObserver(Tab tab) {
        mTab = tab;
        mTab.addObserver(this);
        mScreenshotAction = SCREENSHOT_ACTION_NONE;
    }

    @Override
    public void destroy() {
        mTab.removeObserver(this);
        mTab = null;
    }

    @Override
    public void onClosingStateChanged(Tab tab, boolean closing) {
        reportScreenshotUMA(tab);
    }

    @Override
    public void onDestroyed(Tab tab) {
        reportScreenshotUMA(tab);
    }

    @Override
    public void onLoadStarted(Tab tab, boolean toDifferentDocument) {
        reportScreenshotUMA(tab);
    }

    @Override
    public void onActivityAttachmentChanged(Tab tab, @Nullable WindowAndroid window) {
        // Intentionally do nothing to prevent automatic observer removal on detachment.
    }

    public void onActionPerformedAfterScreenshot(int action) {
        if (mScreenshotsTaken > 0) mScreenshotAction = action;
    }

    public void onScreenshotTaken() {
        RecordUserAction.record("Tab.Screenshot");
        mScreenshotsTaken++;
    }

    /** Before leaving a page, report screenshot related UMA and reset screenshot counter. */
    private void reportScreenshotUMA(Tab tab) {
        if (mScreenshotsTaken > 0) {
            RecordHistogram.recordCount1MHistogram(
                    "Tab.Screenshot.ScreenshotsPerPage", mScreenshotsTaken);
            RecordHistogram.recordEnumeratedHistogram(
                    "Tab.Screenshot.Action", mScreenshotAction, SCREENSHOT_ACTION_COUNT);
            // For UKM, report a boolean metric as to whether a screenshot was
            // taken.
            WebContents webContents = tab.getWebContents();
            if (webContents != null) {
                new UkmRecorder.Bridge()
                        .recordEventWithBooleanMetric(webContents, "Tab.Screenshot", "HasOccurred");
            }
        }

        mScreenshotsTaken = 0;
        mScreenshotAction = SCREENSHOT_ACTION_NONE;
        if (mOnReportCompleteForTesting != null) {
            mOnReportCompleteForTesting.run();
            mOnReportCompleteForTesting = null;
        }
    }

    public void setOnReportCompleteForTesting(Runnable onReportCompleteForTesting) {
        mOnReportCompleteForTesting = onReportCompleteForTesting;
    }
}
