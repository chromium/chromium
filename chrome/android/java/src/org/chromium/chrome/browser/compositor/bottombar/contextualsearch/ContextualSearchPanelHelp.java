// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.bottombar.contextualsearch;

import android.animation.Animator;
import android.animation.AnimatorListenerAdapter;
import android.content.Context;
import android.view.View;
import android.view.ViewGroup;
import android.widget.Button;

import androidx.annotation.Nullable;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.MathUtils;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.compositor.bottombar.OverlayPanel;
import org.chromium.chrome.browser.compositor.bottombar.OverlayPanelAnimation;
import org.chromium.chrome.browser.compositor.bottombar.OverlayPanelInflater;
import org.chromium.chrome.browser.compositor.bottombar.contextualsearch.ContextualSearchPanel.ContextualSearchHelpSectionHost;
import org.chromium.chrome.browser.layouts.animation.CompositorAnimator;
import org.chromium.ui.base.LocalizationUtils;
import org.chromium.ui.resources.dynamics.DynamicResourceLoader;

/**
 * Controls a section of the Panel that provides end user help messages.
 * This class is implemented along the lines of {@code ContextualSearchPromoControl} and shares
 * some of it's resources.
 */
public class ContextualSearchPanelHelp {
    private static final int INVALID_VIEW_ID = 0;

    private final boolean mIsEnabled;
    private final OverlayPanel mOverlayPanel;
    private final Context mContext;

    /** The pixel density. */
    private final float mDpToPx;

    /** The background color of the view's container to use for native rendering. */
    private final int mContainerBackgroundColor;

    /**
     * The inflated View, or {@code null} if the associated Feature is not enabled,
     * or {@link #destroy} has been called.
     */
    @Nullable
    private HelpControlView mHelpControlView;

    /** Whether the view is visible. */
    private boolean mIsVisible;

    /** The opacity of the view. */
    private float mOpacity;

    /** The height of the view in pixels. */
    private float mHeightPx;

    /** The height of the content in pixels. */
    private float mContentHeightPx;

    /** Whether the view is showing. */
    private boolean mIsShowingView;

    /** The Y position of the view. */
    private float mViewY;

    /** Whether the View was in a state that could be interacted. */
    private boolean mWasInteractive;

    /** The reference to the help section host. */
    private ContextualSearchHelpSectionHost mHelpSectionHost;

    /**
     * @param panel             The panel.
     * @param helpSectionHost   A reference the host of this section for notifications.
     * @param context           The Android Context used to inflate the View.
     * @param container         The container View used to inflate the View.
     * @param resourceLoader    The resource loader that will handle the snapshot capturing.
     */
    ContextualSearchPanelHelp(OverlayPanel panel, ContextualSearchHelpSectionHost helpSectionHost,
            Context context, ViewGroup container, DynamicResourceLoader resourceLoader) {
        mIsEnabled = helpSectionHost.isPanelHelpEnabled();
        mDpToPx = context.getResources().getDisplayMetrics().density;
        // We match the Opt-in promo background color so the views are seamless when together.
        mContainerBackgroundColor = ApiCompatibilityUtils.getColor(
                context.getResources(), R.color.contextual_search_promo_background_color);

        mOverlayPanel = panel;
        mContext = context;

        mHelpControlView =
                mIsEnabled ? new HelpControlView(panel, context, container, resourceLoader) : null;
        mHelpSectionHost = helpSectionHost;
    }

    // ============================================================================================
    // Public API
    // ============================================================================================

    /**
     * @return Whether the View is visible.
     */
    public boolean isVisible() {
        return mIsVisible;
    }

    /**
     * @return The View height in pixels.
     */
    public float getHeightPx() {
        return mIsVisible ? mHeightPx : 0f;
    }

    /** Returns the ID of the help view, or {@code INVALID_VIEW_ID} if there's no view. */
    public int getViewId() {
        return mHelpControlView != null ? mHelpControlView.getViewId() : INVALID_VIEW_ID;
    }

    /**
     * @return The View opacity.
     */
    public float getOpacity() {
        return mOpacity;
    }

    /**
     * @return The background color of the View's container, which includes areas outside the
     *         content.
     */
    public int getContainerBackgroundColor() {
        return mContainerBackgroundColor;
    }

    // ============================================================================================
    // Package-private API
    // ============================================================================================

    /**
     * Shows the View. This includes inflating the View and setting its initial state.
     */
    void show() {
        if (mIsVisible || !mIsEnabled) return;

        // Invalidates the View in order to generate a snapshot, but do not show the View yet.
        // The View should only be displayed when in the expanded state.
        if (mHelpControlView != null) mHelpControlView.invalidate();

        mIsVisible = true;
        mWasInteractive = false;

        mHeightPx = mContentHeightPx;
    }

    /**
     * Hides the View
     */
    void hide() {
        if (!mIsVisible) return;

        hideView();

        mIsVisible = false;

        mHeightPx = 0.f;
        mOpacity = 0.f;
    }

    /**
     * @return Whether the View reached a state in which it could be interactive.
     */
    boolean wasInteractive() {
        // TODO(donnd): call this and record metrics for whether the user interacted.
        return mWasInteractive;
    }

    // ============================================================================================
    // Panel Animation
    // ============================================================================================

    /**
     * Interpolates the UI from states Closed to Peeked.
     *
     * @param percentage The completion percentage.
     */
    void onUpdateFromCloseToPeek(float percentage) {
        if (!isVisible()) return;

        // The View snapshot should be fully visible here.
        updateAppearance(1.0f);

        // The View should not be visible in this state.
        hideView();
    }

    /**
     * Interpolates the UI from states Peeked to Expanded.
     *
     * @param percentage The completion percentage.
     */
    void onUpdateFromPeekToExpand(float percentage) {
        updateViewAndNativeAppearance(percentage);
    }

    /**
     * Interpolates the UI from states Expanded to Maximized.
     *
     * @param percentage The completion percentage.
     */
    void onUpdateFromExpandToMaximize(float percentage) {
        updateViewAndNativeAppearance(percentage);
    }

    /**
     * Destroys as much of this instance as possible.
     */
    void destroy() {
        if (mHelpControlView != null) mHelpControlView.destroy();
        mHelpControlView = null;
    }

    /** Invalidates the help view. */
    void invalidate(boolean didViewSizeChange) {
        if (mHelpControlView != null) mHelpControlView.invalidate(didViewSizeChange);
    }

    /**
     * Updates the Android View's hidden state and the native appearance attributes.
     * This hides the Android View when transitioning between states and instead shows the
     * native snapshot (by setting the native attributes to do so).
     * @param percentage The completion percentage.
     */
    private void updateViewAndNativeAppearance(float percentage) {
        if (!isVisible()) return;

        // The panel stays full sized during this size transition.
        updateAppearance(1.0f);

        // We should show the View only when the Panel has reached the exact height.
        // If not exact then the non-interactive native code draws the Panel during the transition.
        if (percentage == 1.f) {
            showView();
        } else {
            hideView();
        }
    }

    // ============================================================================================
    // View Acceptance Animation
    // ============================================================================================

    /**
     * Collapses the View in an animated fashion.
     */
    void collapse() {
        if (!mIsShowingView) return;

        hideView();

        // Notify the host that the content is moving so adjustments can be made (e.g. the Opt-in
        // promo will be in motion when it's shown).
        mHelpSectionHost.onPanelSectionSizeChange(true);

        CompositorAnimator collapse =
                CompositorAnimator.ofFloat(mOverlayPanel.getAnimationHandler(), 1.f, 0.f,
                        OverlayPanelAnimation.BASE_ANIMATION_DURATION_MS, null);

        collapse.addUpdateListener(animator -> updateAppearance(animator.getAnimatedValue()));

        collapse.addListener(new AnimatorListenerAdapter() {
            @Override
            public void onAnimationEnd(Animator animation) {
                hide();
                mHelpSectionHost.onPanelSectionSizeChange(false);
            }
        });

        collapse.start();
    }

    /**
     * Updates the appearance of the View.
     * @param percentage The completion percentage. 0.f means the view is fully collapsed and
     *        transparent. 1.f means the view is fully expanded and opaque.
     */
    private void updateAppearance(float percentage) {
        if (mIsVisible) {
            mHeightPx = Math.round(
                    MathUtils.clamp(percentage * mContentHeightPx, 0.f, mContentHeightPx));
            mOpacity = 1.f;
        } else {
            mHeightPx = 0.f;
            mOpacity = 0.f;
        }
    }

    // ============================================================================================
    // Helpers
    // ============================================================================================

    /**
     * Shows the Android View. By making the Android View visible, we are allowing the
     * View to be interactive. Since snapshots are not interactive (they are just a bitmap),
     * we need to temporarily show the Android View on top of the snapshot, so the user will
     * be able to click in the View button.
     */
    private void showView() {
        if (mHelpControlView == null) return;

        float y = mHelpSectionHost.getYPositionPx();
        View view = mHelpControlView.getHelpView();
        if (view == null || !mIsVisible || (mIsShowingView && mViewY == y) || mHeightPx == 0.f) {
            return;
        }

        float offsetX = mOverlayPanel.getOffsetX() * mDpToPx;
        if (LocalizationUtils.isLayoutRtl()) {
            offsetX = -offsetX;
        }

        view.setTranslationX(offsetX);
        view.setTranslationY(y);
        view.setVisibility(View.VISIBLE);

        // NOTE: We need to call requestLayout, otherwise the View will not become visible.
        view.requestLayout();

        mIsShowingView = true;
        mViewY = y;

        // The View can only be interactive when it is being displayed.
        mWasInteractive = true;
    }

    /**
     * Hides the Android View. See {@link #showView()}.
     */
    private void hideView() {
        if (mHelpControlView == null) return;

        View view = mHelpControlView.getHelpView();
        if (view == null || !mIsVisible || !mIsShowingView) {
            return;
        }

        view.setVisibility(View.INVISIBLE);

        mIsShowingView = false;
    }

    /**
     * Calculates the content height of the View, and adjusts the height of the View while
     * preserving the proportion of the height with the content height. This should be called
     * whenever the the size of the View changes.
     */
    private void calculateHeight() {
        if (mHelpControlView == null) return;

        mHelpControlView.layoutView();

        final float previousContentHeight = mContentHeightPx;
        mContentHeightPx = mHelpControlView.getMeasuredHeight();

        if (mIsVisible) {
            // Calculates the ratio between the current height and the previous content height,
            // and uses it to calculate the new height, while preserving the ratio.
            final float ratio = mHeightPx / previousContentHeight;
            mHeightPx = Math.round(mContentHeightPx * ratio);
        }
    }

    /** Called when the OK button has been clicked. */
    private void onOkButtonClicked() {
        mHelpSectionHost.onPanelHelpOkClicked();
        collapse();
    }

    // ============================================================================================
    // HelpControlView - the View that this class delegates to.
    // ============================================================================================

    /**
     * The {@code HelpControlView} is an {@link OverlayPanelInflater} controlled view that renders
     * the actual help View and can be created and destroyed under the control of the enclosing
     * Panel Help class. The enclosing class delegates several public mehthods to this class, e.g.
     * {@link #invalidate}.
     */
    private class HelpControlView extends OverlayPanelInflater {
        /**
         * Constructs a help view that can be shown in the panel.
         * @param panel             The panel.
         * @param context           The Android Context used to inflate the View.
         * @param container         The container View used to inflate the View.
         * @param resourceLoader    The resource loader that will handle the snapshot capturing.
         */
        HelpControlView(OverlayPanel panel, Context context, ViewGroup container,
                DynamicResourceLoader resourceLoader) {
            super(panel, R.layout.contextual_search_panel_help_view,
                    R.id.contextual_search_panel_help, context, container, resourceLoader);
        }

        View getHelpView() {
            return super.getView();
        }

        void layoutView() {
            super.layout();
        }

        @Override
        public void destroy() {
            hide();
            super.destroy();
        }

        @Override
        public void invalidate(boolean didViewSizeChange) {
            super.invalidate(didViewSizeChange);

            if (didViewSizeChange) {
                calculateHeight();
            }
        }

        @Override
        protected void onFinishInflate() {
            super.onFinishInflate();

            View view = getView();

            // "OK" button.
            Button button = (Button) view.findViewById(R.id.contextual_search_ok_button);
            button.setOnClickListener(v -> onOkButtonClicked());

            calculateHeight();
        }

        @Override
        protected boolean shouldDetachViewAfterCapturing() {
            return false;
        }
    }
}
