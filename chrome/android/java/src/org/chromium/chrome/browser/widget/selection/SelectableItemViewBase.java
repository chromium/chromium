// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.widget.selection;

import android.content.Context;
import android.util.AttributeSet;
import android.view.MotionEvent;
import android.view.View;
import android.view.View.OnClickListener;
import android.view.View.OnLongClickListener;
import android.view.View.OnTouchListener;
import android.widget.Checkable;

import androidx.annotation.Nullable;

import org.chromium.chrome.browser.widget.selection.SelectionDelegate.SelectionObserver;
import org.chromium.ui.widget.ViewLookupCachingFrameLayout;

import java.util.List;

/**
 * Provides a generic base class for representing an item that can be selected. When selected, the
 * view will be updated to indicate that it is selected. The exact UI changes for selection state
 * should be provided by the implementing class.
 *
 * A selection is initially established via long-press. If a selection is already established,
 * clicking on the item will toggle its selection.
 *
 * @param <E> The type of the item associated with this SelectableItemViewBase.
 */
public abstract class SelectableItemViewBase<E> extends ViewLookupCachingFrameLayout
        implements Checkable, OnClickListener, OnLongClickListener, OnTouchListener,
                   SelectionObserver<E> {
    // Heuristic value used to rule out long clicks preceded by long horizontal move. A long click
    // is ignored if finger was moved horizontally more than this threshold.
    private static final float LONG_CLICK_SLIDE_THRESHOLD_PX = 100.f;

    private SelectionDelegate<E> mSelectionDelegate;
    private E mItem;
    private @Nullable Boolean mIsChecked;

    // Controls whether selection should happen during onLongClick.
    private boolean mSelectOnLongClick = true;

    // X position of touch events to detect the amount of horizontal movement between touch down
    // and the position where long click is triggered.
    private float mAnchorX;
    private float mCurrentX;

    /**
     * Constructor for inflating from XML.
     */
    public SelectableItemViewBase(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    /**
     * Destroys and cleans up itself.
     */
    public void destroy() {
        if (mSelectionDelegate != null) {
            mSelectionDelegate.removeObserver(this);
        }
    }

    /**
     * Sets the SelectionDelegate and registers this object as an observer. The SelectionDelegate
     * must be set before the item can respond to click events.
     * @param delegate The SelectionDelegate that will inform this item of selection changes.
     */
    public void setSelectionDelegate(SelectionDelegate<E> delegate) {
        if (mSelectionDelegate != delegate) {
            if (mSelectionDelegate != null) mSelectionDelegate.removeObserver(this);
            mSelectionDelegate = delegate;
            mSelectionDelegate.addObserver(this);
        }
    }

    /**
     * Controls whether selection happens during onLongClick or onClick.
     * @param selectOnLongClick True if selection should happen on longClick, false if selection
     *                          should happen on click instead.
     */
    public void setSelectionOnLongClick(boolean selectOnLongClick) {
        mSelectOnLongClick = selectOnLongClick;
    }

    /**
     * @param item The item associated with this SelectableItemViewBase.
     */
    public void setItem(E item) {
        mItem = item;
        setChecked(mSelectionDelegate.isItemSelected(item));
    }

    /**
     * @return The item associated with this SelectableItemViewBase.
     */
    public E getItem() {
        return mItem;
    }

    // FrameLayout implementations.
    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();

        setOnTouchListener(this);
        setOnClickListener(this);
        setOnLongClickListener(this);
    }

    @Override
    protected void onAttachedToWindow() {
        super.onAttachedToWindow();
        if (mSelectionDelegate != null) {
            setChecked(mSelectionDelegate.isItemSelected(mItem));
        }
    }

    @Override
    protected void onDetachedFromWindow() {
        super.onDetachedFromWindow();
        resetCheckedState();
    }

    // OnTouchListener implementation.
    @Override
    public final boolean onTouch(View view, MotionEvent event) {
        int action = event.getActionMasked();
        if (action == MotionEvent.ACTION_DOWN) {
            // mCurrentX needs init here as well, since we might not get ACTION_MOVE
            // for a simple click turning into a long click when selection mode is on.
            mAnchorX = mCurrentX = event.getX();
        } else if (action == MotionEvent.ACTION_MOVE) {
            mCurrentX = event.getX();
        }
        return false;
    }

    // OnClickListener implementation.
    @Override
    public void onClick(View view) {
        assert view == this;

        if (!mSelectOnLongClick) {
            handleSelection();
            return;
        }

        if (isSelectionModeActive()) {
            onLongClick(view);
        } else {
            onClick();
        }
    }

    // OnLongClickListener implementation.
    @Override
    public boolean onLongClick(View view) {
        assert view == this;
        if (Math.abs(mCurrentX - mAnchorX) < LONG_CLICK_SLIDE_THRESHOLD_PX) handleSelection();
        return true;
    }

    private void handleSelection() {
        boolean checked = toggleSelectionForItem(mItem);
        setChecked(checked);
    }

    /**
     * @return Whether we are currently in selection mode.
     */
    protected boolean isSelectionModeActive() {
        return mSelectionDelegate.isSelectionEnabled();
    }

    /**
     * Toggles the selection state for a given item.
     * @param item The given item.
     * @return Whether the item was in selected state after the toggle.
     */
    protected boolean toggleSelectionForItem(E item) {
        return mSelectionDelegate.toggleSelectionForItem(item);
    }

    // Checkable implementations.
    @Override
    public boolean isChecked() {
        return mIsChecked != null && mIsChecked;
    }

    @Override
    public void toggle() {
        setChecked(!isChecked());
    }

    /**
     * Sets whether the item is checked. Note that if the views to be updated run animations, you
     * should override {@link #updateView(boolean)} to get the correct animation state instead of
     * overriding this method to update the views.
     * @param checked Whether the item is checked.
     */
    @Override
    public void setChecked(boolean checked) {
        if (mIsChecked != null && checked == mIsChecked) return;

        // We shouldn't run the animation when mIsChecked is first initialized to the correct state.
        final boolean animate = mIsChecked != null;
        mIsChecked = checked;
        updateView(animate);
    }

    /**
     * Resets the checked state to be uninitialized.
     */
    private void resetCheckedState() {
        setChecked(false);
        mIsChecked = null;
    }

    // SelectionObserver implementation.
    @Override
    public void onSelectionStateChange(List<E> selectedItems) {
        setChecked(mSelectionDelegate.isItemSelected(mItem));
    }

    /**
     * Update the view based on whether this item is selected.
     * @param animate Whether to animate the selection state changing if applicable.
     */
    protected void updateView(boolean animate) {}

    /**
     * Same as {@link OnClickListener#onClick(View)} on this.
     * Subclasses should override this instead of setting their own OnClickListener because this
     * class handles onClick events in selection mode, and won't forward events to subclasses in
     * that case.
     */
    protected abstract void onClick();
}
