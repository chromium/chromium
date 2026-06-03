// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.ntp_customization.theme_sync;

import android.content.Context;
import android.view.View;
import android.view.ViewGroup;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.ntp_customization.BottomSheetDelegate;

/** Coordinator for the NTP theme sync history. */
@NullMarked
public class NtpThemeSyncHistoryCoordinator {
    public NtpThemeSyncHistoryCoordinator(
            Context context,
            ViewGroup parentView,
            BottomSheetDelegate bottomSheetDelegate,
            View.OnClickListener moreOptionsClickListener) {}

    /** Prepare data before showing the NTP theme history. */
    public void prepareToShow() {}

    /** Called to destroy the NtpThemeSyncHistoryCoordinator. */
    public void destroy() {}
}
