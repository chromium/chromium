// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.android_webview.devui;

import android.os.SystemClock;

import androidx.fragment.app.Fragment;

import org.chromium.base.metrics.RecordHistogram;

/** A base class for all fragments in the UI. */
public abstract class DevUiBaseFragment extends Fragment {
    private long mStartOfSession;

    /**
     * Called by {@link MainActivity} to to give the fragment an opportunity to show an error
     * message. It's up to the {@link MainActivity} to call this or not.
     * Populates and show the error message in the given {@code errorView} or NOOP if the
     * fragment has no errors to display. Creating and showing the error message may be an
     * async operation.
     */
    /* package */ void maybeShowErrorView(PersistentErrorView errorView) {
        errorView.hide();
    }

    @Override
    public void onResume() {
        super.onResume();
        mStartOfSession = SystemClock.elapsedRealtime();
    }

    private void logSessionDuration() {
        String suffix = "Unknown";
        if (this instanceof HomeFragment) {
            suffix = "HomeFragment";
        } else if (this instanceof FlagsFragment) {
            suffix = "FlagsFragment";
        } else if (this instanceof CrashesListFragment) {
            suffix = "CrashesListFragment";
        } else if (this instanceof ComponentsListFragment) {
            suffix = "ComponentsListFragment";
        } else if (this instanceof SafeModeFragment) {
            suffix = "SafeModeFragment";
        } else if (this instanceof NetLogsFragment) {
            suffix = "NetLogsFragment";
        }
        // Note: keep this if-else ladder synchronized with the AndroidWebViewFragments
        // histogram_suffix
        long endOfSession = SystemClock.elapsedRealtime();
        RecordHistogram.recordLongTimesHistogram100(
                "Android.WebView.DevUi.SessionDuration2." + suffix, endOfSession - mStartOfSession);
    }

    @Override
    public void onPause() {
        logSessionDuration();
        super.onPause();
    }
}
