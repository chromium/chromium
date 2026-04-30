// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.android_webview.devui;

import android.os.Bundle;
import android.os.SystemClock;
import android.view.View;

import androidx.activity.OnBackPressedCallback;
import androidx.fragment.app.Fragment;

import org.chromium.base.DeviceInfo;
import org.chromium.base.metrics.RecordHistogram;

/** A base class for all fragments in the UI. */
public abstract class DevUiBaseFragment extends Fragment {
    private long mStartOfSession;
    private boolean mShouldRequestFocus;
    private Boolean mIsTV;

    /**
     * @return true if the device is an Android TV.
     */
    public boolean isTV() {
        if (mIsTV == null) {
            mIsTV = DeviceInfo.isTV();
        }
        return mIsTV;
    }

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
    }

    protected void registerBackPressToNavBarCallback(View navBarButton) {
        requireActivity()
                .getOnBackPressedDispatcher()
                .addCallback(
                        getViewLifecycleOwner(),
                        new OnBackPressedCallback(true) {
                            @Override
                            public void handleOnBackPressed() {
                                navBarButton.requestFocus();
                            }
                        });
    }

    /**
     * Set whether the fragment should request focus on its own when it's resumed.
     *
     * @param shouldRequestFocus true if the fragment should request focus.
     */
    public void setShouldRequestFocus(boolean shouldRequestFocus) {
        assert isTV() : "Focus can only be requested for TV devices";
        mShouldRequestFocus = shouldRequestFocus;
    }

    /**
     * @return true if the fragment should request focus.
     */
    protected boolean shouldRequestFocus() {
        assert isTV() : "Focus can only be requested for TV devices";
        return mShouldRequestFocus;
    }

    /**
     * Called by {@link MainActivity} to to give the fragment an opportunity to show an error
     * message. It's up to the {@link MainActivity} to call this or not. Populates and show the
     * error message in the given {@code errorView} or NOOP if the fragment has no errors to
     * display. Creating and showing the error message may be an async operation.
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

    protected int getSelectableItemBackgroundResId() {
        android.util.TypedValue outValue = new android.util.TypedValue();
        getContext()
                .getTheme()
                .resolveAttribute(android.R.attr.selectableItemBackground, outValue, true);
        return outValue.resourceId;
    }
}
