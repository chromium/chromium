// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp;

import android.content.Context;
import android.util.AttributeSet;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewStub;
import android.widget.LinearLayout;

import org.chromium.base.Log;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;

/**
 * Layout for the new tab page. This positions the page elements in the correct vertical positions.
 * There are no separate phone and tablet UIs; this layout adapts based on the available space.
 * TODO(https://crbug.com/487641528): Rename to NewTabPageLayout.
 */
@NullMarked
public class NtpLayout extends LinearLayout {
    /** Delegate to handle layout-related events. */
    public interface Delegate {
        /**
         * Called during the {@link #onMeasure(int, int)} pass.
         *
         * @param width The measured width of the layout.
         */
        void onMeasure(int width);

        /** Called when the layout is attached to a window. */
        void onAttachedToWindow();

        /** Called when the layout's window visibility changes to {@link View#VISIBLE}. */
        void updateActionButtonVisibility();
    }

    private static final String TAG = "NewTabPageLayout";

    private @Nullable Delegate mDelegate;

    /** Constructor for inflating from XML. */
    public NtpLayout(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();

        setBackgroundColor(
                getResources()
                        .getColor(R.color.home_surface_background_color, getContext().getTheme()));

        // TODO(crbug.com/347509698): Remove the log statements after fixing the bug.
        Log.i(TAG, "NewTabPageLayout.onFinishInflate before insertSiteSectionView");

        initializeSiteSectionView();

        Log.i(TAG, "NewTabPageLayout.onFinishInflate after insertSiteSectionView");
    }

    @Override
    protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
        if (mDelegate != null) {
            int width = MeasureSpec.getSize(widthMeasureSpec);
            mDelegate.onMeasure(width);
        }

        super.onMeasure(widthMeasureSpec, heightMeasureSpec);
    }

    @Override
    protected void onAttachedToWindow() {
        super.onAttachedToWindow();
        if (mDelegate != null) {
            mDelegate.onAttachedToWindow();
        }
    }

    @Override
    protected void onWindowVisibilityChanged(int visibility) {
        super.onWindowVisibilityChanged(visibility);

        if (visibility == VISIBLE && mDelegate != null) {
            mDelegate.updateActionButtonVisibility();
        }
    }

    private void initializeSiteSectionView() {
        var mvTilesContainerLayout =
                (ViewGroup) ((ViewStub) findViewById(R.id.mv_tiles_layout_stub)).inflate();
        mvTilesContainerLayout.setVisibility(View.VISIBLE);
        // The page contents are initially hidden; otherwise they'll be drawn centered on the
        // page before the tiles are available and then jump upwards to make space once the
        // tiles are available.
        if (getVisibility() != View.VISIBLE) setVisibility(View.VISIBLE);
    }

    /** Sets the delegate instance. */
    void setDelegate(Delegate delegate) {
        mDelegate = delegate;
    }
}
