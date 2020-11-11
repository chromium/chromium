// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions;

import android.content.res.Resources;
import android.graphics.Color;
import android.graphics.Rect;
import android.graphics.drawable.ColorDrawable;
import android.graphics.drawable.Drawable;
import android.os.Build;
import android.view.InputDevice;
import android.view.MotionEvent;
import android.view.View;
import android.view.View.MeasureSpec;
import android.view.ViewGroup;
import android.view.ViewParent;
import android.view.ViewTreeObserver.OnGlobalLayoutListener;
import android.view.WindowInsets;

import androidx.annotation.NonNull;

import org.chromium.base.task.PostTask;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.components.browser_ui.styles.ChromeColors;
import org.chromium.content_public.browser.UiThreadTaskTraits;
import org.chromium.ui.base.ViewUtils;

/** Common functionality of the Omnibox suggestions dropdown. */
class OmniboxSuggestionsDropdownDelegate implements View.OnAttachStateChangeListener {
    private ViewGroup mSuggestionsDropdown;
    private OmniboxSuggestionsDropdownEmbedder mEmbedder;
    private OmniboxSuggestionsDropdown.Observer mObserver;
    private View mAnchorView;
    private View mAlignmentView;
    private OnGlobalLayoutListener mAnchorViewLayoutListener;
    private View.OnLayoutChangeListener mAlignmentViewLayoutListener;

    private final int mStandardBgColor;
    private final int mIncognitoBgColor;

    private int mListViewMaxHeight;
    private int mLastBroadcastedListViewMaxHeight;

    private final int[] mTempPosition = new int[2];
    private final Rect mTempRect = new Rect();

    OmniboxSuggestionsDropdownDelegate(Resources resources, ViewGroup suggestionsDropdown) {
        mSuggestionsDropdown = suggestionsDropdown;
        mSuggestionsDropdown.addOnAttachStateChangeListener(this);

        mStandardBgColor = ChromeColors.getDefaultThemeColor(resources, false);
        mIncognitoBgColor = ChromeColors.getDefaultThemeColor(resources, true);
    }

    /** Sets the observer of suggestions dropdown. */
    public void setObserver(OmniboxSuggestionsDropdown.Observer observer) {
        mObserver = observer;
    }

    /** Sets the embedder of the dropdown and creates necessary listeners. */
    public void setEmbedder(OmniboxSuggestionsDropdownEmbedder embedder) {
        assert mEmbedder == null;
        mEmbedder = embedder;
        mAnchorView = mEmbedder.getAnchorView();
        // Prior to Android M, the contextual actions associated with the omnibox were anchored to
        // the top of the screen and not a floating copy/paste menu like on newer versions.  As a
        // result of this, the toolbar is pushed down in these Android versions and we need to
        // montior those changes to update the positioning of the list.
        mAnchorViewLayoutListener = new OnGlobalLayoutListener() {
            private int mOffsetInWindow;
            private WindowInsets mWindowInsets;
            private final Rect mTempRect = new Rect();
            private final Rect mWindowRect = new Rect();

            @Override
            public void onGlobalLayout() {
                int offsetInWindow = 0;
                View currentView = mAnchorView;
                while (true) {
                    offsetInWindow += currentView.getTop();
                    ViewParent parent = currentView.getParent();
                    if (parent == null || !(parent instanceof View)) break;
                    currentView = (View) parent;
                }

                boolean insetsHaveChanged = false;
                WindowInsets currentInsets = null;
                // TODO(ender): Replace with VERSION_CODE_R once we switch to SDK 30.
                if (Build.VERSION.SDK_INT >= 30) {
                    currentInsets = mAnchorView.getRootWindowInsets();
                    insetsHaveChanged = !currentInsets.equals(mWindowInsets);
                } else if (isAdaptiveSuggestionsCountEnabled()) {
                    mEmbedder.getWindowDelegate().getWindowVisibleDisplayFrame(mTempRect);
                    insetsHaveChanged = !mTempRect.equals(mWindowRect);
                    mWindowRect.set(mTempRect);
                }

                if (mOffsetInWindow == offsetInWindow && !insetsHaveChanged) return;
                mWindowInsets = currentInsets;
                mOffsetInWindow = offsetInWindow;
                mSuggestionsDropdown.requestLayout();
            }
        };

        mAlignmentView = mEmbedder.getAlignmentView();
        if (mAlignmentView != null) {
            mAlignmentViewLayoutListener = new View.OnLayoutChangeListener() {
                @Override
                public void onLayoutChange(View v, int left, int top, int right, int bottom,
                        int oldLeft, int oldTop, int oldRight, int oldBottom) {
                    adjustSidePadding();
                }
            };
        } else {
            mAlignmentViewLayoutListener = null;
        }
    }

    // Implementation of View.OnAttachStateChangeListener.
    @Override
    public void onViewAttachedToWindow(View view) {
        mAnchorView.getViewTreeObserver().addOnGlobalLayoutListener(mAnchorViewLayoutListener);
        if (mAlignmentView != null) {
            adjustSidePadding();
            mAlignmentView.addOnLayoutChangeListener(mAlignmentViewLayoutListener);
        }
    }

    @Override
    public void onViewDetachedFromWindow(View view) {
        mAnchorView.getViewTreeObserver().removeOnGlobalLayoutListener(mAnchorViewLayoutListener);
        if (mAlignmentView != null) {
            mAlignmentView.removeOnLayoutChangeListener(mAlignmentViewLayoutListener);
        }
    }

    private void adjustSidePadding() {
        if (mAlignmentView == null) return;

        ViewUtils.getRelativeLayoutPosition(mAnchorView, mAlignmentView, mTempPosition);
        mSuggestionsDropdown.setPadding(mTempPosition[0], mSuggestionsDropdown.getPaddingTop(),
                mAnchorView.getWidth() - mAlignmentView.getWidth() - mTempPosition[0],
                mSuggestionsDropdown.getPaddingBottom());
    }

    /**
     * Provides an appropriate background drawable.
     * @param isIncognito whether the request is made in off the record context.
     * @return a drawable with appropriate color.
     */
    public Drawable getPopupBackground(boolean isIncognito) {
        int color = isIncognito ? mIncognitoBgColor : mStandardBgColor;
        if (!mSuggestionsDropdown.isHardwareAccelerated()) {
            // When HW acceleration is disabled, changing mSuggestionList' items somehow erases
            // mOmniboxResultsContainer' background from the area not covered by mSuggestionList.
            // To make sure mOmniboxResultsContainer is always redrawn, we make list background
            // color slightly transparent. This makes mSuggestionList.isOpaque() to return false,
            // and forces redraw of the parent view (mOmniboxResultsContainer).
            if (Color.alpha(color) == 255) {
                color = Color.argb(254, Color.red(color), Color.green(color), Color.blue(color));
            }
        }
        return new ColorDrawable(color);
    }

    /**
     * Calculates width and height of the {@link OmniboxSuggestionsDropdown} taking into account
     * embedder elements, such as anchor view and alignment view. This method will also trigger an
     * update if the viewport height changes.
     * @param outMeasureSpecs the result, which should be an array with 2 elements.
     */
    public void calculateOnMeasureAndTriggerUpdates(@NonNull int[] outMeasureSpecs) {
        assert outMeasureSpecs != null && outMeasureSpecs.length == 2;

        int anchorBottomRelativeToContent = calculateAnchorBottomRelativeToContent();
        maybeUpdateLayoutParams(anchorBottomRelativeToContent);

        int availableViewportHeight =
                calculateAvailableViewportHeight(anchorBottomRelativeToContent);
        notifyObserversIfViewportHeightChanged(availableViewportHeight);

        outMeasureSpecs[0] =
                MeasureSpec.makeMeasureSpec(mAnchorView.getMeasuredWidth(), MeasureSpec.EXACTLY);
        outMeasureSpecs[1] = MeasureSpec.makeMeasureSpec(availableViewportHeight,
                mEmbedder.isTablet() ? MeasureSpec.AT_MOST : MeasureSpec.EXACTLY);
    }

    private int calculateAnchorBottomRelativeToContent() {
        View contentView =
                mEmbedder.getAnchorView().getRootView().findViewById(android.R.id.content);
        ViewUtils.getRelativeLayoutPosition(contentView, mAnchorView, mTempPosition);
        int anchorY = mTempPosition[1];
        return anchorY + mAnchorView.getMeasuredHeight();
    }

    private void maybeUpdateLayoutParams(int topMargin) {
        // Update the layout params to ensure the parent correctly positions the suggestions under
        // the anchor view.
        ViewGroup.LayoutParams layoutParams = mSuggestionsDropdown.getLayoutParams();
        if (layoutParams != null && layoutParams instanceof ViewGroup.MarginLayoutParams) {
            ((ViewGroup.MarginLayoutParams) layoutParams).topMargin = topMargin;
        }
    }

    private int calculateAvailableViewportHeight(int anchorBottomRelativeToContent) {
        mEmbedder.getWindowDelegate().getWindowVisibleDisplayFrame(mTempRect);
        return mTempRect.height() - anchorBottomRelativeToContent;
    }

    private void notifyObserversIfViewportHeightChanged(int availableViewportHeight) {
        if (availableViewportHeight == mListViewMaxHeight) return;

        mListViewMaxHeight = availableViewportHeight;
        if (mObserver != null) {
            PostTask.postTask(UiThreadTaskTraits.DEFAULT, () -> {
                // Detect if there was another change since this task posted.
                // This indicates a subsequent task being posted too.
                if (mListViewMaxHeight != availableViewportHeight) return;
                // Detect if the new height is the same as previously broadcasted.
                // The two checks (one above and one below) allow us to detect quick
                // A->B->A transitions and suppress the broadcasts.
                if (mLastBroadcastedListViewMaxHeight == availableViewportHeight) return;
                if (mObserver == null) return;

                mObserver.onSuggestionDropdownHeightChanged(availableViewportHeight);
                mLastBroadcastedListViewMaxHeight = availableViewportHeight;
            });
        }
    }

    /**
     * Whether we should ignore the {@link MotionEvent}.
     * @param event the motion event that could be ignored.
     */
    public boolean shouldIgnoreGenericMotionEvent(MotionEvent event) {
        // Consume mouse events to ensure clicks do not bleed through to sibling views that
        // are obscured by the list.  crbug.com/968414
        if ((event.getSource() & InputDevice.SOURCE_CLASS_POINTER) != 0
                && event.getToolType(0) == MotionEvent.TOOL_TYPE_MOUSE) {
            int action = event.getActionMasked();
            if (action == MotionEvent.ACTION_BUTTON_PRESS
                    || action == MotionEvent.ACTION_BUTTON_RELEASE) {
                return true;
            }
        }
        return false;
    }

    /** @return Whether Adaptive Suggestions Count feature is enabled. */
    private boolean isAdaptiveSuggestionsCountEnabled() {
        return ChromeFeatureList.isInitialized() &&
                ChromeFeatureList.isEnabled(ChromeFeatureList.OMNIBOX_ADAPTIVE_SUGGESTIONS_COUNT);
    }
}
