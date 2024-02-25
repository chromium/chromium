// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.home.view;

import android.content.Context;
import android.util.AttributeSet;
import android.view.LayoutInflater;
import android.widget.FrameLayout;
import android.widget.ImageView;

import androidx.vectordrawable.graphics.drawable.AnimatedVectorDrawableCompat;

import org.chromium.chrome.browser.download.internal.R;

/**
 * A helper UI widget that provides visual feedback when the selection state of the underlying view
 * is changed. The widget represents three distinct states : selected, in selection mode and not
 * selected. The caller can define the UI behavior at each of these states by subclassing this view.
 */
public class SelectionView extends FrameLayout {
    private final ImageView mCheck;
    private final ImageView mCircle;
    private final AnimatedVectorDrawableCompat mCheckDrawable;

    private boolean mIsSelected;
    private boolean mInSelectionMode;
    private boolean mShowSelectedAnimation;

    /** Constructor for inflating from XML. */
    public SelectionView(Context context, AttributeSet attrs) {
        super(context, attrs);
        LayoutInflater.from(context).inflate(R.layout.list_selection_handle_view, this, true);
        mCheck = findViewById(R.id.check);
        mCircle = findViewById(R.id.circle);
        mCheckDrawable =
                AnimatedVectorDrawableCompat.create(
                        context, R.drawable.ic_check_googblue_24dp_animated);
    }

    @Override
    public boolean isSelected() {
        return mIsSelected;
    }

    /** @return Whether the selection mode is currently active. */
    public boolean isInSelectionMode() {
        return mInSelectionMode;
    }

    /**
     * Called to inform the view about its current selection state.
     * @param selected Whether the item is currently selected.
     * @param inSelectionMode Whether we are currently in active selection mode.
     * @param showSelectedAnimation Whether the item was recently selected from an unselected state
     * and animation should be shown.
     */
    public void setSelectionState(
            boolean selected, boolean inSelectionMode, boolean showSelectedAnimation) {
        mIsSelected = selected;
        mInSelectionMode = inSelectionMode;
        mShowSelectedAnimation = showSelectedAnimation;
        updateView();
    }

    private void updateView() {
        if (mIsSelected) {
            mCheck.setVisibility(VISIBLE);
            mCircle.setVisibility(GONE);

            mCheck.setImageDrawable(mCheckDrawable);
            mCheck.getBackground()
                    .setLevel(getResources().getInteger(R.integer.list_item_level_selected));
            if (mShowSelectedAnimation) mCheckDrawable.start();
        } else if (mInSelectionMode) {
            mCheck.setVisibility(GONE);
            mCircle.setVisibility(VISIBLE);
        } else {
            mCheck.setVisibility(GONE);
            mCircle.setVisibility(GONE);
        }
    }
}
