// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.actions;

import android.content.Context;
import android.view.View;

import androidx.annotation.StringRes;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.user_education.IphCommandBuilder;
import org.chromium.chrome.browser.user_education.UserEducationHelper;
import org.chromium.components.browser_ui.widget.highlight.ViewHighlighter.HighlightParams;
import org.chromium.components.browser_ui.widget.highlight.ViewHighlighter.HighlightShape;

/**
 * Encapsulates the data required to request an IPH (In-Product Help) bubble. Guarantees that the
 * IPH intent is only consumed and shown once, even if bound to multiple UI surfaces.
 */
@NullMarked
public class IphIntent {
    private final String mFeatureName;
    private final @StringRes int mStringId;
    private final @StringRes int mAccessibilityStringId;
    private final Object @Nullable [] mStringArgs;
    private final @Nullable HighlightParams mHighlightParams;
    private final @Nullable Runnable mOnShowCallback;
    private final @Nullable Runnable mOnDismissCallback;
    private final long mAutoDismissTimeoutMs;
    private final boolean mEnableSnoozeMode;

    // Tracks whether this exact IPH intent has already been consumed by a view.
    private boolean mHasBeenShown;

    private IphIntent(Builder builder) {
        mFeatureName = builder.mFeatureName;
        mStringId = builder.mStringId;
        mAccessibilityStringId = builder.mAccessibilityStringId;
        mStringArgs = builder.mStringArgs;
        mHighlightParams = builder.mHighlightParams;
        mOnShowCallback = builder.mOnShowCallback;
        mOnDismissCallback = builder.mOnDismissCallback;
        mAutoDismissTimeoutMs = builder.mAutoDismissTimeoutMs;
        mEnableSnoozeMode = builder.mEnableSnoozeMode;
    }

    /** Returns true if this IPH has already been consumed and requested by a view. */
    public boolean hasBeenShown() {
        return mHasBeenShown;
    }

    /** Returns the feature name associated with this IPH intent for testing. */
    public String getFeatureNameForTesting() {
        return mFeatureName;
    }

    /**
     * Attempts to build and show the IPH anchored to the provided view. Returns true if
     * successfully requested, false if it has already been shown.
     */
    public boolean tryShow(View anchorView, UserEducationHelper userEducationHelper) {
        // Enforce single consumption
        if (mHasBeenShown) return false;
        mHasBeenShown = true;

        Context context = anchorView.getContext();
        IphCommandBuilder builder;

        if (mStringArgs != null && mStringArgs.length > 0) {
            String text = context.getString(mStringId, mStringArgs);
            String a11yText = context.getString(mAccessibilityStringId, mStringArgs);
            builder = new IphCommandBuilder(context.getResources(), mFeatureName, text, a11yText);
        } else {
            builder =
                    new IphCommandBuilder(
                            context.getResources(),
                            mFeatureName,
                            mStringId,
                            mAccessibilityStringId);
        }

        builder.setAnchorView(anchorView);

        if (mHighlightParams != null) {
            builder.setHighlightParams(mHighlightParams);
        }
        if (mOnShowCallback != null) {
            builder.setOnShowCallback(mOnShowCallback);
        }
        if (mOnDismissCallback != null) {
            builder.setOnDismissCallback(mOnDismissCallback);
        }
        if (mAutoDismissTimeoutMs > 0) {
            builder.setAutoDismissTimeout((int) mAutoDismissTimeoutMs);
        }
        if (mEnableSnoozeMode) {
            builder.setEnableSnoozeMode(true);
        }

        userEducationHelper.requestShowIph(builder.build());
        return true;
    }

    /** Configuration for highlight params to bridge the gap for callers needing a config object. */
    public static class HighlightConfig {
        public final HighlightShape shape;
        public final boolean boundsRespectPadding;

        public HighlightConfig(HighlightShape shape, boolean boundsRespectPadding) {
            this.shape = shape;
            this.boundsRespectPadding = boundsRespectPadding;
        }
    }

    /** Builder for {@link IphIntent}. */
    public static class Builder {
        private final String mFeatureName;
        private @StringRes int mStringId;
        private @StringRes int mAccessibilityStringId;
        private Object @Nullable [] mStringArgs;
        private @Nullable HighlightParams mHighlightParams;
        private @Nullable Runnable mOnShowCallback;
        private @Nullable Runnable mOnDismissCallback;
        private long mAutoDismissTimeoutMs;
        private boolean mEnableSnoozeMode;

        public Builder(String featureName) {
            mFeatureName = featureName;
        }

        public Builder setStringResId(@StringRes int stringId) {
            mStringId = stringId;
            return this;
        }

        public Builder setAccessibilityResId(@StringRes int accessibilityStringId) {
            mAccessibilityStringId = accessibilityStringId;
            return this;
        }

        public Builder setStringArgs(Object... stringArgs) {
            mStringArgs = stringArgs;
            return this;
        }

        public Builder setHighlightParams(HighlightParams highlightParams) {
            mHighlightParams = highlightParams;
            return this;
        }

        public Builder setHighlightConfig(HighlightConfig config) {
            mHighlightParams = new HighlightParams(config.shape);
            mHighlightParams.setBoundsRespectPadding(config.boundsRespectPadding);
            return this;
        }

        public Builder setOnShowCallback(Runnable onShowCallback) {
            mOnShowCallback = onShowCallback;
            return this;
        }

        public Builder setOnDismissCallback(Runnable onDismissCallback) {
            mOnDismissCallback = onDismissCallback;
            return this;
        }

        public Builder setAutoDismissTimeoutMs(long autoDismissTimeoutMs) {
            mAutoDismissTimeoutMs = autoDismissTimeoutMs;
            return this;
        }

        public Builder setEnableSnoozeMode(boolean enableSnoozeMode) {
            mEnableSnoozeMode = enableSnoozeMode;
            return this;
        }

        public IphIntent build() {
            return new IphIntent(this);
        }
    }
}
