// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.explore_sites;

import android.content.Context;
import android.os.Parcel;
import android.os.Parcelable;
import android.support.v7.widget.LinearLayoutManager;
import android.support.v7.widget.RecyclerView;
import android.view.View;

import androidx.annotation.Nullable;

/**
 * Custom layout manager that fixes the scrollbar size based on number of items
 * to provide a scrollbar utility that will not shift as a recylcer view scrolls
 * between items of different heights.
 */
class StableScrollLayoutManager extends LinearLayoutManager {
    // Fixes the scrollbar size so it will not resize.
    private int mScrollValue;
    private SavedState mPendingSavedState;

    public static class SavedState implements Parcelable {
        static final int DEFAULT_FOCUSED_CHILD = 0;

        // Non-null when this is saved and restored as a result of onSaveInstanceState.
        @Nullable
        private Parcelable mLinearLayoutManagerState;

        public int focusedCategoryCardPosition;
        public int focusedTileIndex = DEFAULT_FOCUSED_CHILD;

        public static final Creator<SavedState> CREATOR = new Creator<SavedState>() {
            @Override
            public SavedState createFromParcel(Parcel in) {
                return new SavedState(in);
            }

            @Override
            public SavedState[] newArray(int size) {
                return new SavedState[size];
            }
        };

        SavedState() {}

        SavedState(Parcelable linearLayoutManagerState) {
            this.mLinearLayoutManagerState = linearLayoutManagerState;
        }

        SavedState(Parcel in) {
            this.focusedCategoryCardPosition = in.readInt();
            this.focusedTileIndex = in.readInt();
            this.mLinearLayoutManagerState = in.readParcelable(getClass().getClassLoader());
        }

        /**
         * Creates a saved state from another already existing state, and updates the linear
         * layout manager state. This is useful if we are saving state while there is a state
         * waiting to be restored, since the most recent linear layout manager state should be
         * copied into the new saved state.
         */
        SavedState(SavedState other, Parcelable linearLayoutManagerState) {
            this.focusedCategoryCardPosition = other.focusedCategoryCardPosition;
            this.focusedTileIndex = other.focusedTileIndex;
            this.mLinearLayoutManagerState = linearLayoutManagerState;
        }

        /** Utility functions for checking the validity of the category card to focus. */
        boolean hasValidFocusedCategoryCardPosition() {
            return this.focusedCategoryCardPosition > RecyclerView.NO_POSITION;
        }

        void invalidateFocusedCategoryCardPosition() {
            this.focusedCategoryCardPosition = RecyclerView.NO_POSITION;
        }

        @Override
        public int describeContents() {
            return 0;
        }

        @Override
        public void writeToParcel(Parcel dest, int flags) {
            dest.writeInt(this.focusedCategoryCardPosition);
            dest.writeInt(this.focusedTileIndex);
            dest.writeParcelable(this.mLinearLayoutManagerState, flags);
        }
    }

    StableScrollLayoutManager(Context context) {
        super(context);
        setSmoothScrollbarEnabled(false);
    }

    @Override
    public int computeVerticalScrollExtent(RecyclerView.State state) {
        final int count = getItemCount();
        if (count > 0) {
            mScrollValue = getHeight() / count;
            return mScrollValue;
        }
        return 0;
    }

    @Override
    public int computeVerticalScrollRange(RecyclerView.State state) {
        // Fix the scroll range.
        return Math.max((getItemCount() - 1) * mScrollValue, 0);
    }

    @Override
    public int computeVerticalScrollOffset(RecyclerView.State state) {
        final int count = getChildCount();
        // If this was called before the recycler view fully initialized itself, return 0.
        if (count <= 0) return 0;

        // Snap to bottom if we scrolled to the bottom.
        if (findLastCompletelyVisibleItemPosition() == getItemCount() - 1) {
            return Math.max((getItemCount() - 1) * mScrollValue, 0);
        }

        // Find the first visible view and check that views are properly initialized.
        // This includes if a view was recycled or swapped out just now.
        int firstPos = findFirstVisibleItemPosition();
        if (firstPos == RecyclerView.NO_POSITION) return 0;
        View view = findViewByPosition(firstPos);
        if (view == null) return 0;

        // Top of the view in pixels
        final int top = getDecoratedTop(view);
        int height = getDecoratedMeasuredHeight(view);
        int heightOfScreen;
        if (height <= 0) {
            heightOfScreen = 0;
        } else {
            heightOfScreen = Math.abs(mScrollValue * top / height);
        }
        if (heightOfScreen == 0 && firstPos > 0) {
            return mScrollValue * firstPos - 1;
        }
        return (mScrollValue * firstPos) + heightOfScreen;
    }

    @Override
    public Parcelable onSaveInstanceState() {
        Parcelable superState = super.onSaveInstanceState();
        if (mPendingSavedState != null
                && mPendingSavedState.hasValidFocusedCategoryCardPosition()) {
            return new SavedState(mPendingSavedState, superState);
        }

        SavedState state = new SavedState(superState);
        ExploreSitesCategoryCardView focusedCategory = categoryFromView(getFocusedChild());
        if (focusedCategory == null) {
            state.invalidateFocusedCategoryCardPosition();
        } else {
            state.focusedCategoryCardPosition = getPosition(focusedCategory);
            state.focusedTileIndex =
                    focusedCategory.getFocusedTileIndex(SavedState.DEFAULT_FOCUSED_CHILD);
        }
        return state;
    }

    @Override
    public void onRestoreInstanceState(Parcelable state) {
        if (state instanceof SavedState) {
            mPendingSavedState = (SavedState) state;
            super.onRestoreInstanceState(mPendingSavedState.mLinearLayoutManagerState);
        }
    }

    @Override
    public void onLayoutCompleted(RecyclerView.State state) {
        super.onLayoutCompleted(state);
        if (mPendingSavedState != null
                && mPendingSavedState.hasValidFocusedCategoryCardPosition()) {
            focusTileAtPosition(mPendingSavedState.focusedCategoryCardPosition,
                    mPendingSavedState.focusedTileIndex);
        }
        mPendingSavedState = null;
    }

    /**
     * Scrolls to an adapter position, focusing the first tile.  Actual focus request happens in
     * #onLayoutCompleted.
     *
     * @param position The adapter position to scroll to.
     */
    void scrollToPositionAndFocus(int position) {
        // NOTE: LinearLayoutManager#scrollToPosition has strange behavior if the scrolling
        // happens between the time that the adapter has an item and the time that the view has
        // actually added its children.  In that case, the LinearLayoutManager will only scroll
        // the requested position /into view/.
        //
        // To work around that, we use LinearLayoutManager#scrollToPositionWithOffset, and set
        // the offset to 0.  This allows us to always scroll the desired view to the top of the
        // screen.
        scrollToPositionWithOffset(position, 0);
        mPendingSavedState = new SavedState();
        mPendingSavedState.focusedCategoryCardPosition = position;
        mPendingSavedState.focusedTileIndex = 0;
    }

    /**
     * Focuses a particular tile in the ExploreSitesPage. If the tile is unavailable, returns
     * without focusing anything.
     * @param position The adapter position of the card containing the tile
     * @param tileIndex The index of the desired tile.
     */
    private void focusTileAtPosition(int position, int tileIndex) {
        View itemAtPosition = findViewByPosition(position);
        if (itemAtPosition == null) return;

        ExploreSitesCategoryCardView categoryAtPosition = categoryFromView(itemAtPosition);
        if (categoryAtPosition == null) return;

        View tileToFocus = categoryAtPosition.getTileViewAt(tileIndex);
        if (tileToFocus == null) return;

        tileToFocus.requestFocus();
    }

    private ExploreSitesCategoryCardView categoryFromView(View child) {
        if (child == null) return null;

        int viewType = getItemViewType(child);
        if (viewType != CategoryCardAdapter.ViewType.CATEGORY) return null;

        assert child instanceof ExploreSitesCategoryCardView;

        return (ExploreSitesCategoryCardView) child;
    }
}
