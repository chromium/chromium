// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.fullscreen;

import android.os.Parcel;
import android.os.Parcelable;

import org.chromium.build.annotations.NullMarked;

/** Options to control a fullscreen request. */
@NullMarked
public class FullscreenOptions implements Parcelable {
    /** Whether the navigation bar should be shown. */
    public final boolean showNavigationBar;

    /** Whether the status bar should be shown. */
    public final boolean showStatusBar;

    /** Target display for the fullscreen transition */
    public final long displayId;

    // Used by FullscreenHtmlApiHandler internally to indicate that the fullscreen request
    // associated with this option got canceled at the pending state.
    private boolean mCanceled;

    public static final Parcelable.Creator<FullscreenOptions> CREATOR =
            new Parcelable.Creator<>() {
                @Override
                public FullscreenOptions createFromParcel(Parcel in) {
                    return new FullscreenOptions(in);
                }

                @Override
                public FullscreenOptions[] newArray(int size) {
                    return new FullscreenOptions[size];
                }
            };

    /**
     * Constructs FullscreenOptions.
     *
     * @param showNavigationBar Whether the navigation bar should be shown.
     * @param showStatusBar Whether the status bar should be shown.
     * @param displayId target display id.
     */
    public FullscreenOptions(boolean showNavigationBar, boolean showStatusBar, long displayId) {
        this.showNavigationBar = showNavigationBar;
        this.showStatusBar = showStatusBar;
        this.displayId = displayId;
    }

    public FullscreenOptions(Parcel in) {
        this(in.readBoolean(), in.readBoolean(), in.readLong());
    }

    void setCanceled() {
        mCanceled = true;
    }

    boolean canceled() {
        return mCanceled;
    }

    /** Implements {@link Parcelable} */
    @Override
    public int describeContents() {
        return 0;
    }

    /** Implements {@link Parcelable} */
    @Override
    public void writeToParcel(Parcel out, int flags) {
        out.writeBoolean(showNavigationBar);
        out.writeBoolean(showStatusBar);
        out.writeLong(displayId);
    }

    @Override
    public boolean equals(Object obj) {
        if (!(obj instanceof FullscreenOptions)) {
            return false;
        }
        FullscreenOptions options = (FullscreenOptions) obj;
        return showNavigationBar == options.showNavigationBar
                && showStatusBar == options.showStatusBar
                && displayId == options.displayId;
    }

    @Override
    public String toString() {
        return "FullscreenOptions(showNavigationBar="
                + showNavigationBar
                + ",showStatusBar="
                + showStatusBar
                + ", displayId="
                + displayId
                + ", canceled="
                + mCanceled
                + ")";
    }
}
