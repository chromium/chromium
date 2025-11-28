// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.activity_recreation;

import android.os.Parcel;
import android.os.Parcelable;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

/** Used to preserve and restore the UI state of the activity. */
@NullMarked
final class ActivityRecreationUiState implements Parcelable {
    boolean mIsUrlBarFocused;
    @Nullable String mUrlBarEditText;
    boolean mIsKeyboardShown;
    boolean mIsTabSwitcherShown;
    boolean mIsPointerLocked;
    boolean mIsKeyboardLocked;

    public static final Parcelable.Creator<ActivityRecreationUiState> CREATOR =
            new Parcelable.Creator<>() {
                @Override
                public ActivityRecreationUiState createFromParcel(Parcel in) {
                    return new ActivityRecreationUiState(in);
                }

                @Override
                public ActivityRecreationUiState[] newArray(int size) {
                    return new ActivityRecreationUiState[size];
                }
            };

    /** Constructor of ActivityRecreationUiState using default values. */
    ActivityRecreationUiState() {
        this(
                /* isUrlBarFocused= */ false,
                /* urlBarEditText= */ "",
                /* isKeyboardShown= */ false,
                /* isTabSwitcherShown= */ false,
                /* isPointerLocked= */ false,
                /* isKeyboardLocked= */ false);
    }

    /**
     * Constructor of ActivityRecreationUiState.
     *
     * @param isUrlBarFocused whether url bar is focused.
     * @param urlBarEditText the edit text in url bar.
     * @param isKeyboardShown whether soft keyboard is shown.
     * @param isTabSwitcherShown whether tab switcher is shown.
     */
    ActivityRecreationUiState(
            boolean isUrlBarFocused,
            @Nullable String urlBarEditText,
            boolean isKeyboardShown,
            boolean isTabSwitcherShown,
            boolean isPointerLocked,
            boolean isKeyboardLocked) {
        mIsUrlBarFocused = isUrlBarFocused;
        mUrlBarEditText = urlBarEditText;
        mIsKeyboardShown = isKeyboardShown;
        mIsTabSwitcherShown = isTabSwitcherShown;
        mIsPointerLocked = isPointerLocked;
        mIsKeyboardLocked = isKeyboardLocked;
    }

    private ActivityRecreationUiState(Parcel in) {
        this(
                /* isUrlBarFocused= */ in.readInt() == 1,
                /* urlBarEditText= */ in.readString(),
                /* isKeyboardShown= */ in.readInt() == 1,
                /* isTabSwitcherShown= */ in.readInt() == 1,
                /* mIsPointerLocked */ in.readInt() == 1,
                /* mIsKeyboardLocked */ in.readInt() == 1);
    }

    /** Implements {@link Parcelable} */
    @Override
    public int describeContents() {
        return 0;
    }

    /** Implements {@link Parcelable} */
    @Override
    public void writeToParcel(Parcel out, int flags) {
        out.writeInt(mIsUrlBarFocused ? 1 : 0);
        out.writeString(mUrlBarEditText);
        out.writeInt(mIsKeyboardShown ? 1 : 0);
        out.writeInt(mIsTabSwitcherShown ? 1 : 0);
        out.writeInt(mIsPointerLocked ? 1 : 0);
        out.writeInt(mIsKeyboardLocked ? 1 : 0);
    }

    boolean shouldRetainState() {
        return mIsUrlBarFocused
                || mIsKeyboardShown
                || mIsTabSwitcherShown
                || mIsPointerLocked
                || mIsKeyboardLocked;
    }
}
