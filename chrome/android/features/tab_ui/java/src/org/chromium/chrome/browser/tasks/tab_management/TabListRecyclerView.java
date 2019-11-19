// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.chromium.chrome.features.start_surface.StartSurfaceLayout.ZOOMING_DURATION;

import android.animation.Animator;
import android.animation.AnimatorListenerAdapter;
import android.animation.ObjectAnimator;
import android.animation.ValueAnimator;
import android.annotation.SuppressLint;
import android.content.Context;
import android.content.res.Resources;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Rect;
import android.os.SystemClock;
import android.support.v7.content.res.AppCompatResources;
import android.support.v7.widget.RecyclerView;
import android.util.AttributeSet;
import android.view.Gravity;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewParent;
import android.widget.FrameLayout;
import android.widget.ImageView;
import android.widget.RelativeLayout;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.Log;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.tab.TabFeatureUtilities;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.tab_ui.R;
import org.chromium.ui.interpolators.BakedBezierInterpolator;
import org.chromium.ui.modelutil.SimpleRecyclerViewAdapter;
import org.chromium.ui.resources.dynamics.DynamicResourceLoader;
import org.chromium.ui.resources.dynamics.ViewResourceAdapter;
import org.chromium.ui.widget.ViewLookupCachingFrameLayout;

/**
 * A custom RecyclerView implementation for the tab grid, to handle show/hide logic in class.
 */
class TabListRecyclerView extends RecyclerView {
    private static final String TAG = "TabListRecyclerView";

    private static final String MAX_DUTY_CYCLE_PARAM = "max-duty-cycle";
    private static final float DEFAULT_MAX_DUTY_CYCLE = 0.2f;

    public static final long BASE_ANIMATION_DURATION_MS = 218;
    public static final long FINAL_FADE_IN_DURATION_MS = 50;

    /**
     * Field trial parameter for downsampling scaling factor.
     */
    private static final String DOWNSAMPLING_SCALE_PARAM = "downsampling-scale";

    private static final float DEFAULT_DOWNSAMPLING_SCALE = 0.5f;

    /**
     * An interface to listen to visibility related changes on this {@link RecyclerView}.
     */
    interface VisibilityListener {
        /**
         * Called before the animation to show the tab list has started.
         * @param isAnimating Whether visibility is changing with animation
         */
        void startedShowing(boolean isAnimating);

        /**
         * Called when the animation to show the tab list is finished.
         */
        void finishedShowing();

        /**
         * Called before the animation to hide the tab list has started.
         * @param isAnimating Whether visibility is changing with animation
         */
        void startedHiding(boolean isAnimating);

        /**
         * Called when the animation to show the tab list is finished.
         */
        void finishedHiding();
    }

    private class TabListOnScrollListener extends RecyclerView.OnScrollListener {
        @Override
        public void onScrolled(RecyclerView recyclerView, int dx, int dy) {
            final int yOffset = recyclerView.computeVerticalScrollOffset();
            if (yOffset == 0) {
                setShadowVisibility(false);
                return;
            }

            if (dy == 0 || recyclerView.getScrollState() == SCROLL_STATE_SETTLING) return;

            setShadowVisibility(yOffset > 0);
        }
    }

    private final int mResourceId;
    private ValueAnimator mFadeInAnimator;
    private ValueAnimator mFadeOutAnimator;
    private VisibilityListener mListener;
    private DynamicResourceLoader mLoader;
    private ViewResourceAdapter mDynamicView;
    private boolean mIsDynamicViewRegistered;
    private long mLastDirtyTime;
    private RecyclerView.ItemAnimator mOriginalAnimator;
    private ImageView mShadowImageView;
    private int mShadowTopMargin;
    private TabListOnScrollListener mScrollListener;
    private View mRecyclerViewFooter;
    private Rect mOriginalPadding;

    /**
     * Basic constructor to use during inflation from xml.
     */
    public TabListRecyclerView(Context context, AttributeSet attributeSet) {
        super(context, attributeSet);

        // Use this object in case there are multiple instances of this class.
        mResourceId = this.toString().hashCode();
    }

    /**
     * Set the {@link VisibilityListener} that will listen on granular visibility events.
     * @param listener The {@link VisibilityListener} to use.
     */
    void setVisibilityListener(VisibilityListener listener) {
        mListener = listener;
    }

    void prepareOverview() {
        endAllAnimations();

        registerDynamicView();

        // Stop all the animations to make all the items show up and scroll to position immediately.
        mOriginalAnimator = getItemAnimator();
        setItemAnimator(null);
    }

    /**
     * Start showing the tab list.
     * @param animate Whether the visibility change should be animated.
     */
    void startShowing(boolean animate) {
        assert mFadeOutAnimator == null;
        mListener.startedShowing(animate);

        long duration = TabFeatureUtilities.isTabToGtsAnimationEnabled()
                ? FINAL_FADE_IN_DURATION_MS
                : BASE_ANIMATION_DURATION_MS;

        setAlpha(0);
        setVisibility(View.VISIBLE);
        mFadeInAnimator = ObjectAnimator.ofFloat(this, View.ALPHA, 1);
        mFadeInAnimator.setInterpolator(BakedBezierInterpolator.FADE_IN_CURVE);
        mFadeInAnimator.setDuration(duration);
        mFadeInAnimator.start();
        mFadeInAnimator.addListener(new AnimatorListenerAdapter() {
            @Override
            public void onAnimationEnd(Animator animation) {
                mFadeInAnimator = null;
                mListener.finishedShowing();
                // Restore the original value.
                setItemAnimator(mOriginalAnimator);
                setShadowVisibility(computeVerticalScrollOffset() > 0);
                if (mDynamicView != null) {
                    mDynamicView.dropCachedBitmap();
                    unregisterDynamicView();
                }
                if (mRecyclerViewFooter != null) {
                    mRecyclerViewFooter.setVisibility(VISIBLE);
                }
                // TODO(crbug.com/972157): remove this band-aid after we know why GTS is invisible.
                if (TabFeatureUtilities.isTabToGtsAnimationEnabled()) {
                    requestLayout();
                }
            }
        });
        if (!animate) mFadeInAnimator.end();
    }

    void setShadowVisibility(boolean shouldShowShadow) {
        if (mShadowImageView == null) {
            Context context = getContext();
            mShadowImageView = new ImageView(context);
            mShadowImageView.setImageDrawable(AppCompatResources.getDrawable(
                    context, org.chromium.chrome.R.drawable.modern_toolbar_shadow));
            mShadowImageView.setScaleType(ImageView.ScaleType.FIT_XY);
            Resources res = context.getResources();
            if (getParent() instanceof FrameLayout) {
                // Add shadow for grid tab switcher.
                FrameLayout.LayoutParams params =
                        new FrameLayout.LayoutParams(LayoutParams.MATCH_PARENT,
                                res.getDimensionPixelSize(
                                        org.chromium.chrome.R.dimen.toolbar_shadow_height),
                                Gravity.TOP);
                params.topMargin = mShadowTopMargin;
                mShadowImageView.setLayoutParams(params);
                FrameLayout parent = (FrameLayout) getParent();
                parent.addView(mShadowImageView);
            } else if (getParent() instanceof RelativeLayout) {
                // Add shadow for tab grid dialog.
                RelativeLayout parent = (RelativeLayout) getParent();
                View toolbar = parent.getChildAt(0);
                if (!(toolbar instanceof TabGroupUiToolbarView)) return;

                RelativeLayout.LayoutParams params =
                        new RelativeLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT,
                                res.getDimensionPixelSize(
                                        org.chromium.chrome.R.dimen.toolbar_shadow_height));
                params.addRule(RelativeLayout.BELOW, toolbar.getId());
                parent.addView(mShadowImageView, params);
            }
        }

        if (shouldShowShadow && mShadowImageView.getVisibility() != VISIBLE) {
            mShadowImageView.setVisibility(VISIBLE);
        } else if (!shouldShowShadow && mShadowImageView.getVisibility() != GONE) {
            mShadowImageView.setVisibility(GONE);
        }
    }

    void setShadowTopMargin(int shadowTopMargin) {
        mShadowTopMargin = shadowTopMargin;
    }

    /**
     * @return The ID for registering and using the dynamic resource in compositor.
     */
    int getResourceId() {
        return mResourceId;
    }

    long getLastDirtyTimeForTesting() {
        return mLastDirtyTime;
    }

    private float getDownsamplingScale() {
        String scale = ChromeFeatureList.getFieldTrialParamByFeature(
                ChromeFeatureList.TAB_TO_GTS_ANIMATION, DOWNSAMPLING_SCALE_PARAM);
        try {
            return Float.valueOf(scale);
        } catch (NumberFormatException e) {
            return DEFAULT_DOWNSAMPLING_SCALE;
        }
    }

    /**
     * Create a DynamicResource for this RecyclerView.
     * The view resource can be obtained by {@link #getResourceId} in compositor layer.
     */
    void createDynamicView(DynamicResourceLoader loader) {
        mDynamicView = new ViewResourceAdapter(this) {
            private long mSuppressedUntil;

            @Override
            public boolean isDirty() {
                boolean dirty = super.isDirty();
                if (dirty) {
                    mLastDirtyTime = SystemClock.elapsedRealtime();
                }
                if (SystemClock.elapsedRealtime() < mSuppressedUntil) {
                    if (dirty) {
                        Log.d(TAG, "Dynamic View is dirty but suppressed");
                    }
                    return false;
                }
                return dirty;
            }

            @Override
            public Bitmap getBitmap() {
                long startTime = SystemClock.elapsedRealtime();
                Bitmap bitmap = super.getBitmap();
                long elapsed = SystemClock.elapsedRealtime() - startTime;
                if (elapsed == 0) elapsed = 1;

                float maxDutyCycle = getMaxDutyCycle();
                Log.d(TAG, "MaxDutyCycle = " + getMaxDutyCycle());
                assert maxDutyCycle > 0;
                assert maxDutyCycle <= 1;
                long suppressedFor = Math.min(
                        (long) (elapsed * (1 - maxDutyCycle) / maxDutyCycle), ZOOMING_DURATION);

                mSuppressedUntil = SystemClock.elapsedRealtime() + suppressedFor;
                Log.d(TAG, "DynamicView: spent %dms on getBitmap, suppress updating for %dms.",
                        elapsed, suppressedFor);
                return bitmap;
            }
        };
        mDynamicView.setDownsamplingScale(getDownsamplingScale());
        assert mLoader == null : "createDynamicView should only be called once";
        mLoader = loader;
    }

    private float getMaxDutyCycle() {
        String maxDutyCycle = ChromeFeatureList.getFieldTrialParamByFeature(
                ChromeFeatureList.TAB_GRID_LAYOUT_ANDROID, MAX_DUTY_CYCLE_PARAM);
        try {
            return Float.valueOf(maxDutyCycle);
        } catch (NumberFormatException e) {
            return DEFAULT_MAX_DUTY_CYCLE;
        }
    }

    private void registerDynamicView() {
        if (mIsDynamicViewRegistered) return;
        if (mLoader == null) return;

        mLoader.registerResource(getResourceId(), mDynamicView);
        mIsDynamicViewRegistered = true;
    }

    private void unregisterDynamicView() {
        if (!mIsDynamicViewRegistered) return;
        if (mLoader == null) return;

        mLoader.unregisterResource(getResourceId());
        mIsDynamicViewRegistered = false;
    }

    @SuppressLint("NewApi") // Used on O+, invalidateChildInParent used for previous versions.
    @Override
    public void onDescendantInvalidated(View child, View target) {
        super.onDescendantInvalidated(child, target);
        if (mDynamicView != null) {
            mDynamicView.invalidate(null);
        }
    }

    @Override
    public ViewParent invalidateChildInParent(int[] location, Rect dirty) {
        ViewParent retVal = super.invalidateChildInParent(location, dirty);
        if (mDynamicView != null) {
            mDynamicView.invalidate(dirty);
        }
        return retVal;
    }

    @Override
    protected void onAttachedToWindow() {
        super.onAttachedToWindow();

        mScrollListener = new TabListOnScrollListener();
        addOnScrollListener(mScrollListener);
    }

    @Override
    protected void onDetachedFromWindow() {
        super.onDetachedFromWindow();

        if (mShadowImageView != null) {
            removeViewInLayout(mShadowImageView);
            mShadowImageView = null;
        }

        if (mScrollListener != null) {
            removeOnScrollListener(mScrollListener);
            mScrollListener = null;
        }
    }

    @Override
    public void onDraw(Canvas c) {
        super.onDraw(c);
        if (mRecyclerViewFooter == null || getVisibility() != View.VISIBLE) return;
        // Always put the recyclerView footer below the recyclerView if there is one.
        ViewHolder viewHolder = findViewHolderForAdapterPosition(getAdapter().getItemCount() - 1);
        if (viewHolder == null) {
            mRecyclerViewFooter.setVisibility(INVISIBLE);
        } else {
            if (mRecyclerViewFooter.getVisibility() != VISIBLE) {
                mRecyclerViewFooter.setVisibility(VISIBLE);
            }
            mRecyclerViewFooter.setY(
                    viewHolder.itemView.getBottom() + mRecyclerViewFooter.getHeight());
        }
    }

    /**
     * Start hiding the tab list.
     * @param animate Whether the visibility change should be animated.
     */
    void startHiding(boolean animate) {
        endAllAnimations();

        registerDynamicView();

        mListener.startedHiding(animate);
        mFadeOutAnimator = ObjectAnimator.ofFloat(this, View.ALPHA, 0);
        mFadeOutAnimator.setInterpolator(BakedBezierInterpolator.FADE_OUT_CURVE);
        mFadeOutAnimator.setDuration(BASE_ANIMATION_DURATION_MS);
        mFadeOutAnimator.addListener(new AnimatorListenerAdapter() {
            @Override
            public void onAnimationEnd(Animator animation) {
                mFadeOutAnimator = null;
                setVisibility(View.INVISIBLE);
                mListener.finishedHiding();
                if (mRecyclerViewFooter != null) {
                    mRecyclerViewFooter.setVisibility(INVISIBLE);
                }
            }
        });
        setShadowVisibility(false);
        mFadeOutAnimator.start();
        if (!animate) mFadeOutAnimator.end();
    }

    void postHiding() {
        if (mDynamicView != null) {
            mDynamicView.dropCachedBitmap();
            unregisterDynamicView();
        }
    }

    private void endAllAnimations() {
        if (mFadeInAnimator != null) {
            mFadeInAnimator.end();
        }
        if (mFadeOutAnimator != null) {
            mFadeOutAnimator.end();
        }
    }

    /**
     * @param selectedTabIndex The index in the RecyclerView of the selected tab.
     * @param selectedTabId The tab ID of the selected tab.
     * @return The {@link Rect} of the thumbnail of the current tab, relative to the
     *         {@link TabListRecyclerView} coordinates.
     */
    @Nullable
    Rect getRectOfCurrentThumbnail(int selectedTabIndex, int selectedTabId) {
        SimpleRecyclerViewAdapter.ViewHolder holder =
                (SimpleRecyclerViewAdapter.ViewHolder) findViewHolderForAdapterPosition(
                        selectedTabIndex);
        if (holder == null || selectedTabIndex == TabModel.INVALID_TAB_INDEX) return null;
        assert holder.model.get(TabProperties.TAB_ID) == selectedTabId;
        ViewLookupCachingFrameLayout root = (ViewLookupCachingFrameLayout) holder.itemView;
        return getRectOfComponent(root.fastFindViewById(R.id.tab_thumbnail));
    }

    private Rect getRectOfComponent(View v) {
        Rect recyclerViewRect = new Rect();
        Rect componentRect = new Rect();
        getGlobalVisibleRect(recyclerViewRect);
        v.getGlobalVisibleRect(componentRect);

        // Get the relative position.
        componentRect.offset(-recyclerViewRect.left, -recyclerViewRect.top);
        return componentRect;
    }

    /**
     * This method finds out the index of the hovered tab's viewHolder in {@code recyclerView}.
     * @param recyclerView   The recyclerview that owns the tabs' viewHolders.
     * @param view           The view of the selected tab.
     * @param dX             The X offset of the selected tab.
     * @param dY             The Y offset of the selected tab.
     * @param threshold      The threshold to judge whether two tabs are overlapped.
     * @return The index of the hovered tab.
     */
    static int getHoveredTabIndex(
            RecyclerView recyclerView, View view, float dX, float dY, float threshold) {
        for (int i = 0; i < recyclerView.getAdapter().getItemCount(); i++) {
            ViewHolder viewHolder = recyclerView.findViewHolderForAdapterPosition(i);
            if (viewHolder == null) continue;
            View child = viewHolder.itemView;
            if (child.getLeft() == view.getLeft() && child.getTop() == view.getTop()) {
                continue;
            }
            if (isOverlap(child.getLeft(), child.getTop(), view.getLeft() + dX, view.getTop() + dY,
                        threshold)) {
                return i;
            }
        }
        return -1;
    }

    private static boolean isOverlap(
            float left1, float top1, float left2, float top2, float threshold) {
        return Math.abs(left1 - left2) < threshold && Math.abs(top1 - top2) < threshold;
    }

    /**
     * This method setup the footer of {@code recyclerView}.
     * @param footer  The {@link View} of the footer.
     * TODO(yuezhanggg): Refactor the footer as a item in the recyclerView instead of a separate
     * view. (crbug: 987043)
     */
    void setupRecyclerViewFooter(View footer) {
        if (mRecyclerViewFooter != null) return;
        mRecyclerViewFooter = footer;
        setScrollBarStyle(SCROLLBARS_OUTSIDE_OVERLAY);
        final int height = (int) getResources().getDimension(R.dimen.tab_grid_iph_card_height);
        final int padding = (int) getResources().getDimension(R.dimen.tab_grid_iph_card_margin);
        mOriginalPadding =
                new Rect(getPaddingLeft(), getPaddingTop(), getPaddingRight(), getPaddingBottom());
        setPadding(mOriginalPadding.left, mOriginalPadding.top, mOriginalPadding.right,
                mOriginalPadding.bottom + height + padding);
        mRecyclerViewFooter.setVisibility(INVISIBLE);
    }

    /**
     * This method removes the footer of {@code recyclerView} if there is one.
     */
    void removeRecyclerViewFooter() {
        if (mRecyclerViewFooter == null) return;
        ((ViewGroup) mRecyclerViewFooter.getParent()).removeView(mRecyclerViewFooter);
        mRecyclerViewFooter = null;
        // Restore the recyclerView to its original state.
        assert mOriginalPadding != null;
        setPadding(mOriginalPadding.left, mOriginalPadding.top, mOriginalPadding.right,
                mOriginalPadding.bottom);
        setScrollBarStyle(SCROLLBARS_INSIDE_OVERLAY);
    }

    @VisibleForTesting
    View getRecyclerViewFooterForTesting() {
        return mRecyclerViewFooter;
    }
}
