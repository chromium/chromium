// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.bottombar.contextualsearch;

import android.animation.Animator;
import android.animation.AnimatorListenerAdapter;
import android.content.Context;
import android.os.Handler;
import android.text.method.LinkMovementMethod;
import android.view.View;
import android.view.ViewGroup;
import android.widget.Button;
import android.widget.TextView;

import org.chromium.base.MathUtils;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.compositor.bottombar.OverlayPanel;
import org.chromium.chrome.browser.compositor.bottombar.OverlayPanelAnimation;
import org.chromium.chrome.browser.compositor.bottombar.OverlayPanelInflater;
import org.chromium.chrome.browser.compositor.bottombar.contextualsearch.ContextualSearchPanel.ContextualSearchPromoHost;
import org.chromium.chrome.browser.contextualsearch.ContextualSearchSettingsFragment;
import org.chromium.chrome.browser.contextualsearch.ContextualSearchUma;
import org.chromium.chrome.browser.layouts.animation.CompositorAnimator;
import org.chromium.chrome.browser.settings.SettingsNavigationFactory;
import org.chromium.chrome.browser.ui.theme.ChromeSemanticColorUtils;
import org.chromium.components.browser_ui.settings.SettingsNavigation;
import org.chromium.ui.base.LocalizationUtils;
import org.chromium.ui.base.ViewUtils;
import org.chromium.ui.resources.dynamics.DynamicResourceLoader;
import org.chromium.ui.text.NoUnderlineClickableSpan;
import org.chromium.ui.text.SpanApplier;

/**
 * Controls the Contextual Search Opt-in/out privacy Promo that shows within the Panel just below
 * the Bar for users that have not yet accepted or declined our privacy policy.
 */
public class ContextualSearchPromoControl extends OverlayPanelInflater {
    // The percentage that inicates we've reached full size (for this mode) and are now stationary.
    private static final float STATIONARY_PERCENTAGE = 1.0f;
    // An arbitrary intermediate value in between 0 and 1 that indicates we're in transition.
    private static final float INTERMEDIATE_PERCENTAGE = 0.5f;

    /** The interface used to talk to the Panel. */
    private final ContextualSearchPromoHost mHost;

    /** The pixel density. */
    private final float mDpToPx;

    /** The background color of the promo. */
    private final int mBackgroundColor;

    /** Whether the Promo is visible. */
    private boolean mIsVisible;

    /** The opacity of the Promo. */
    private float mOpacity;

    /** The height of the Promo in pixels. */
    private float mHeightPx;

    /** The height of the Promo content in pixels. */
    private float mContentHeightPx;

    /** Whether the Promo View is showing. */
    private boolean mIsShowingView;

    /** The Y position of the Promo View. */
    private float mPromoViewY;

    /** Whether the Promo was in a state that could be interacted. */
    private boolean mWasInteractive;

    /** Whether the user's choice has been handled. */
    private boolean mHasHandledChoice;

    /**
     * @param panel The panel.
     * @param context The Android Context used to inflate the View.
     * @param container The container View used to inflate the View.
     * @param resourceLoader The resource loader that will handle the snapshot capturing.
     */
    ContextualSearchPromoControl(
            OverlayPanel panel,
            ContextualSearchPromoHost host,
            Context context,
            ViewGroup container,
            DynamicResourceLoader resourceLoader) {
        super(
                panel,
                R.layout.contextual_search_promo_view,
                R.id.contextual_search_promo,
                context,
                container,
                resourceLoader);

        mDpToPx = context.getResources().getDisplayMetrics().density;
        mBackgroundColor =
                ChromeSemanticColorUtils.getContextualSearchPromoBackgroundColor(context);

        mHost = host;
    }

    // ============================================================================================
    // Public API
    // ============================================================================================

    /** Shows the Promo. This includes inflating the View and setting its initial state. */
    void show() {
        if (mIsVisible) return;

        // Invalidates the View in order to generate a snapshot, but do not show the View yet.
        // The View should only be displayed when in the expanded state.
        invalidate();

        mIsVisible = true;
        mWasInteractive = false;

        mHeightPx = mContentHeightPx;
    }

    /** Hides the Promo */
    void hide() {
        if (!mIsVisible) return;

        hidePromoView();

        mIsVisible = false;

        mHeightPx = 0.f;
        mOpacity = 0.f;
    }

    /**
     * Handles change in the Contextual Search preference state.
     *
     * @param isEnabled Whether the feature was enable.
     */
    void onContextualSearchPrefChanged(boolean isEnabled) {
        if (!mIsVisible || !mOverlayPanel.isShowing()) return;

        collapse();
    }

    /**
     * @return Whether the Promo is visible.
     */
    public boolean isVisible() {
        return mIsVisible;
    }

    /**
     * @return Whether the Promo reached a state in which it could be interacted.
     */
    public boolean wasInteractive() {
        return mWasInteractive;
    }

    /**
     * @return The Promo height in pixels.
     */
    public float getHeightPx() {
        return mHeightPx;
    }

    /**
     * @return The Promo opacity.
     */
    public float getOpacity() {
        return mOpacity;
    }

    /**
     * @return The background color for the promo, which controls areas outside the content.
     */
    public int getBackgroundColor() {
        return mBackgroundColor;
    }

    // ============================================================================================
    // Panel Animation
    // ============================================================================================

    /**
     * Interpolates the UI from states Closed to Peeked.
     *
     * @param percentage The completion percentage.
     */
    public void onUpdateFromCloseToPeek(float percentage) {
        if (!isVisible()) return;

        // Promo snapshot should be fully visible here.
        updateAppearance(1.f);

        // The View should not be visible in this state.
        hidePromoView();
    }

    /**
     * Interpolates the UI from states Peeked to Expanded.
     *
     * @param percentage The completion percentage.
     */
    public void onUpdateFromPeekToExpand(float percentage) {
        if (!isVisible()) return;

        // Promo snapshot should be fully visible here.
        updateAppearance(1.f);

        if (percentage == 1.f) {
            // We should show the Promo View only when the Panel
            // has reached the exact expanded height.
            showPromoView();
        } else {
            // Otherwise the View should not be visible.
            hidePromoView();
        }
    }

    /**
     * Interpolates the UI from states Expanded to Maximized.
     *
     * @param percentage The completion percentage.
     */
    public void onUpdateFromExpandToMaximize(float percentage) {
        if (!isVisible()) return;

        // Promo snapshot collapses as the Panel reaches the maximized state.
        updateAppearance(1.f - percentage);

        // The View should not be visible in this state.
        hidePromoView();
    }

    /** Notifies that movement of this panel section hast started or stopped. */
    void onUpdateForMovement(boolean isMovementStarting) {
        if (isMovementStarting) hidePromoView();
        updateAppearance(isMovementStarting ? INTERMEDIATE_PERCENTAGE : STATIONARY_PERCENTAGE);
    }

    // ============================================================================================
    // Promo Acceptance Animation
    // ============================================================================================

    /** Collapses the Promo in an animated fashion. */
    public void collapse() {
        hidePromoView();

        // Notify the host that the content is moving so adjustments can be made (e.g. the Opt-in
        // promo will be in motion if it's shown).
        mHost.onPanelSectionSizeChange(true);

        CompositorAnimator collapse =
                CompositorAnimator.ofFloat(
                        mOverlayPanel.getAnimationHandler(),
                        1.f,
                        0.f,
                        OverlayPanelAnimation.BASE_ANIMATION_DURATION_MS,
                        null);

        collapse.addUpdateListener(animator -> updateAppearance(animator.getAnimatedValue()));

        collapse.addListener(
                new AnimatorListenerAdapter() {
                    @Override
                    public void onAnimationEnd(Animator animation) {
                        hide();
                        mHost.onPanelSectionSizeChange(false);
                    }
                });

        collapse.start();
    }

    /**
     * Updates the appearance of the Promo.
     *
     * @param percentage The completion percentage. 0.f means the Promo is fully collapsed and
     *     transparent. 1.f means the Promo is fully expanded and opaque.
     */
    private void updateAppearance(float percentage) {
        if (mIsVisible) {
            mHeightPx =
                    Math.round(
                            MathUtils.clamp(percentage * mContentHeightPx, 0.f, mContentHeightPx));
            mOpacity = percentage;
        } else {
            mHeightPx = 0.f;
            mOpacity = 0.f;
        }
    }

    // ============================================================================================
    // Custom Behaviors
    // ============================================================================================

    @Override
    public void destroy() {
        hide();
        super.destroy();
    }

    @Override
    public void invalidate(boolean didViewSizeChange) {
        super.invalidate(didViewSizeChange);

        if (didViewSizeChange) {
            onPromoViewSizeChange();
        }
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();

        View view = getView();

        // "Allow" button.
        Button allowButton = view.findViewById(R.id.contextual_search_allow_button);
        allowButton.setOnClickListener(
                v -> ContextualSearchPromoControl.this.handlePromoChoice(true));

        // "No thanks" button.
        Button noThanksButton = view.findViewById(R.id.contextual_search_no_thanks_button);
        noThanksButton.setOnClickListener(
                v -> ContextualSearchPromoControl.this.handlePromoChoice(false));

        // Fill in text with link to Settings.
        TextView promoText = view.findViewById(R.id.contextual_search_promo_text);

        NoUnderlineClickableSpan settingsLink =
                new NoUnderlineClickableSpan(
                        view.getContext(),
                        (View ignored) ->
                                ContextualSearchPromoControl.this.handleClickSettingsLink());

        promoText.setText(
                SpanApplier.applySpans(
                        view.getResources().getString(R.string.contextual_search_promo_description),
                        new SpanApplier.SpanInfo("<link>", "</link>", settingsLink)));
        promoText.setMovementMethod(LinkMovementMethod.getInstance());

        onPromoViewSizeChange();
    }

    @Override
    protected boolean shouldDetachViewAfterCapturing() {
        return false;
    }

    // ============================================================================================
    // Promo Interaction
    // ============================================================================================

    /**
     * Handles the choice made by the user in the Promo.
     *
     * @param hasEnabled Whether the user has chosen to enable the feature.
     */
    private void handlePromoChoice(boolean hasEnabled) {
        if (!mHasHandledChoice) {
            mHasHandledChoice = true;
            mHost.setContextualSearchPromoCardSelection(hasEnabled);
            ContextualSearchUma.logPromoCardChoice(hasEnabled);
        }
    }

    /** Handles a click in the settings link located in the Promo. */
    private void handleClickSettingsLink() {
        new Handler()
                .post(
                        new Runnable() {
                            @Override
                            public void run() {
                                SettingsNavigation settingsNavigation =
                                        SettingsNavigationFactory.createSettingsNavigation();
                                settingsNavigation.startSettings(
                                        getContext(), ContextualSearchSettingsFragment.class);
                            }
                        });
    }

    // ============================================================================================
    // Helpers
    // ============================================================================================

    /**
     * Shows the Promo Android View. By making the Android View visible, we are allowing the Promo
     * to be interactive. Since snapshots are not interactive (they are just a bitmap), we need to
     * temporarily show the Android View on top of the snapshot, so the user will be able to click
     * in the Promo buttons and/or link.
     */
    private void showPromoView() {
        float y = mHost.getYPositionPx();
        View view = getView();
        if (view == null
                || !mIsVisible
                || (mIsShowingView && mPromoViewY == y)
                || mHeightPx == 0.f) {
            return;
        }

        float offsetX = mOverlayPanel.getOffsetX() * mDpToPx;
        if (LocalizationUtils.isLayoutRtl()) {
            offsetX = -offsetX;
        }

        view.setTranslationX(offsetX);
        view.setTranslationY(y);
        view.setVisibility(View.VISIBLE);

        // NOTE(pedrosimonetti): We need to call requestLayout, otherwise
        // the Promo View will not become visible.
        ViewUtils.requestLayout(view, "ContextualSearchPromoControl.showPromoView");

        mIsShowingView = true;
        mPromoViewY = y;

        // The Promo can only be interacted when the View is being displayed.
        mWasInteractive = true;

        mHost.onPromoShown();

        updatePromoHeight();
    }

    /** Hides the Promo Android View. See {@link #showPromoView()}. */
    private void hidePromoView() {
        View view = getView();
        if (view == null || !mIsVisible || !mIsShowingView) {
            return;
        }

        view.setVisibility(View.INVISIBLE);

        mIsShowingView = false;
    }

    /**
     * This should be called whenever the the size of the Promo View changes or something inside the
     * promo changes that could affect the overall size.
     */
    private void onPromoViewSizeChange() {
        layout();
        updatePromoHeight();
    }

    /**
     * Calculates the content height of the Promo View, and adjusts the height of the Promo while
     * preserving the proportion of the height with the content height.
     */
    private void updatePromoHeight() {
        final float previousContentHeight = mContentHeightPx;
        mContentHeightPx = getMeasuredHeight();

        if (mIsVisible) {
            // Calculates the ratio between the current height and the previous content height,
            // and uses it to calculate the new height, while preserving the ratio.
            final float ratio = mHeightPx / previousContentHeight;
            mHeightPx = Math.round(mContentHeightPx * ratio);
        }
    }
}
