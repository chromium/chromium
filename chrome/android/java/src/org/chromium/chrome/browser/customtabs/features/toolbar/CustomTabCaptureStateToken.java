// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs.features.toolbar;

import androidx.annotation.ColorInt;
import androidx.annotation.DrawableRes;
import androidx.annotation.Nullable;

import org.chromium.chrome.browser.toolbar.top.ToolbarSnapshotDifference;

import java.util.Objects;

/**
 * The idea of this class is to hold all of the properties that materially change the way the
 * toolbar looks. If two tokens are identical (no difference is found), then there should be
 * no reason to perform a bitmap capture.
 */
class CustomTabCaptureStateToken {
    private final String mUrl;
    private final String mTitle;
    private final @ColorInt int mBackgroundColor;
    private final @DrawableRes int mSecurityIconRes;
    private @Nullable final Object mAnimationToken;
    private final int mViewWidth;
    private final boolean mMinimizeButtonVisible;
    private final boolean mMinimizeButtonHighlighted;

    public CustomTabCaptureStateToken(
            String url,
            String title,
            @ColorInt int backgroundColor,
            @DrawableRes int securityIconRes,
            boolean isInAnimation,
            int viewWidth,
            boolean minimizeButtonVisible,
            boolean minimizeButtonHighlighted) {
        mUrl = url;
        mTitle = title;
        mBackgroundColor = backgroundColor;
        mSecurityIconRes = securityIconRes;
        // When animations are in progress, tokens should never be equal. Object should use
        // reference equality, resulting in a difference unless both are null or the objects
        // are actually the same object.
        mAnimationToken = isInAnimation ? new Object() : null;
        mViewWidth = viewWidth;
        mMinimizeButtonVisible = minimizeButtonVisible;
        mMinimizeButtonHighlighted = minimizeButtonHighlighted;
    }

    /**
     * Compares two tokens and looks for any difference. If multiple are present only one will
     * be returned. ToolbarSnapshotDifference.NONE indicates the two tokens are the same.
     */
    public @ToolbarSnapshotDifference int getAnyDifference(CustomTabCaptureStateToken that) {
        if (that == null) {
            return ToolbarSnapshotDifference.NULL;
        } else if (!Objects.equals(mUrl, that.mUrl)) {
            return ToolbarSnapshotDifference.URL_TEXT;
        } else if (!Objects.equals(mTitle, that.mTitle)) {
            return ToolbarSnapshotDifference.TITLE_TEXT;
        } else if (mBackgroundColor != that.mBackgroundColor) {
            return ToolbarSnapshotDifference.TINT;
        } else if (mSecurityIconRes != that.mSecurityIconRes) {
            return ToolbarSnapshotDifference.SECURITY_ICON;
        } else if (!Objects.equals(mAnimationToken, that.mAnimationToken)) {
            return ToolbarSnapshotDifference.CCT_ANIMATION;
        } else if (mViewWidth != that.mViewWidth) {
            return ToolbarSnapshotDifference.LOCATION_BAR_WIDTH;
        } else if (mMinimizeButtonVisible != that.mMinimizeButtonVisible
                || mMinimizeButtonHighlighted != that.mMinimizeButtonHighlighted) {
            return ToolbarSnapshotDifference.MINIMIZE_BUTTON;
        } else {
            return ToolbarSnapshotDifference.NONE;
        }
    }
}
