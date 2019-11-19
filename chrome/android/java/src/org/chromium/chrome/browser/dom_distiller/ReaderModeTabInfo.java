// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.dom_distiller;

import android.os.SystemClock;

import org.chromium.chrome.browser.dom_distiller.TabDistillabilityProvider.DistillabilityObserver;
import org.chromium.content_public.browser.WebContentsObserver;

/**
 * This class tracks the per-tab state of reader mode.
 */
class ReaderModeTabInfo {
    // The WebContentsObserver responsible for updates to the distillation status of the tab.
    public WebContentsObserver webContentsObserver;

    // The distillation status of the tab.
    public int status;

    // If the infobar was closed due to the close button.
    public boolean isDismissed;

    // The URL that distiller is using for this tab. This is used to check if a result comes
    // back from distiller and the user has already loaded a new URL.
    public String url;

    // Used to flag the the infobar was shown and recorded by UMA.
    public boolean showInfoBarRecorded;

    // Whether or not the current tab is a Reader Mode page.
    public boolean isViewingReaderModePage;

    // The distillability observer attached to the tab.
    public DistillabilityObserver distillabilityObserver;

    // The time that the user started viewing Reader Mode content.
    private long mViewStartTimeMs;

    /**
     * A notification that the user started viewing Reader Mode.
     */
    public void onStartedReaderMode() {
        isViewingReaderModePage = true;
        mViewStartTimeMs = SystemClock.elapsedRealtime();
    }

    /**
     * A notification that the user is no longer viewing Reader Mode. This could be because of a
     * navigation away from the page, switching tabs, or closing the browser.
     * @return The amount of time in ms that the user spent viewing Reader Mode.
     */
    public long onExitReaderMode() {
        isViewingReaderModePage = false;
        return SystemClock.elapsedRealtime() - mViewStartTimeMs;
    }
}
