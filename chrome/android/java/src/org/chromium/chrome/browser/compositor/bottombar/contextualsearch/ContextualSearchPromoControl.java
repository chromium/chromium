// Copyright 2015 The Chromium Authors. All rights reserved.
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

import org.chromium.chrome.R;
import org.chromium.chrome.browser.compositor.animation.CompositorAnimator;
import org.chromium.chrome.browser.compositor.bottombar.OverlayPanel;
import org.chromium.chrome.browser.compositor.bottombar.OverlayPanelAnimation;
import org.chromium.chrome.browser.compositor.bottombar.OverlayPanelInflater;
import org.chromium.chrome.browser.preferences.PrefServiceBridge;
import org.chromium.chrome.browser.preferences.PreferencesLauncher;
import org.chromium.chrome.browser.preferences.privacy.ContextualSearchPreferenceFragment;
import org.chromium.chrome.browser.util.MathUtils;
import org.chromium.ui.base.LocalizationUtils;
import org.chromium.ui.resources.dynamics.DynamicResourceLoader;
import org.chromium.ui.text.NoUnderlineClickableSpan;
import org.chromium.ui.text.SpanApplier;

/**
 * Controls the Search Promo.
 */
public class ContextualSearchPromoControl extends OverlayPanelInflater {
    /**
     * The pixel density.
     */
    private final float mDpToPx;

    /**
     * Whether the Promo is visible.
     */
    private boolean mIsVisible;

    /**
     * Whether the Promo is mandatory.
     */
    private boolean mIsMandatory;

    /**
     * The opacity of the Promo.
     */
    private float mOpacity;

    /**
     * The height of the Promo in pixels.
     */
    private float mHeightPx;

    /**
     * The height of the Promo content in pixels.
     */
    private float mContentHeightPx;

    /**
     * Whether the Promo View is showing.
     */
    private boolean mIsShowingView;

    /**
     * The Y position of the Promo View.
     */
    private float mPromoViewY;

    /**
     * Whether the Promo was in a state that could be interacted.
     */
    private boolean mWasInteractive;

    /**
     * Whether the user's choice has been handled.
     */
    private boolean mHasHandledChoice;

    /**
     * The interface used to talk to the Panel.
     */
    private ContextualSearchPromoHost mHost;

    /**
     * The delegate that is used to communicate with the Panel.
     */
    public interface ContextualSearchPromoHost {
        /**
         * Notifies that the user has opted in.
         * @param wasMandatory Whether the Promo was mandatory.
         */
        void onPromoOptIn(boolean wasMandatory);

        /**
         * Notifies that the user has opted out.
         */
        void onPromoOptOut();

        /**
         * Notifies that the Promo appearance has changed.
         */
        void onUpdatePromoAppearance();
    }

    /**
     * @param panel             The panel.
     * @param context           The Android Context used to inflate the View.
     * @param container         The container View used to inflate the View.
     * @param resourceLoader    The resource loader that will handle the snapshot capturing.
     */
    public ContextualSearchPromoControl(OverlayPanel panel,
                                        ContextualSearchPromoHost host,
                                        Context context,
                                        ViewGroup container,
                                        DynamicResourceLoader resourceLoader) {
        super(panel, R.layout.contextual_search_promo_view,
                R.id.contextual_search_promo, context, container, resourceLoader);

        mDpToPx = context.getResources().getDisplayMetrics().density;

        mHost = host;
    }

    // ============================================================================================
    // Public API
    // ============================================================================================

    /**
     * Shows the Promo. This includes inflating the View and setting its initial state.
     * @param isMandatory Whether the Promo is mandatory.
     */
    public void show(boolean isMandatory) {
        if (mIsVisible) return;

        // Invalidates the View in order to generate a snapshot, but do not show the View yet.
        // The View should only be displayed when in the expanded state.
        invalidate();

        mIsVisible = true;
        mIsMandatory = isMandatory;
        mWasInteractive = false;

        mHeightPx = mContentHeightPx;
    }

    /**
     * Hides the Promo
     */
    public void hide() {
        if (!mIsVisible) return;

        hidePromoView();

        mIsVisible = false;
        mIsMandatory = false;

        mHeightPx = 0.f;
        mOpacity = 0.f;
    }

    /**
     * Handles change in the Contextual Search preference state.
     * @param isEnabled Whether the feature was enable.
     */
    public void onContextualSearchPrefChanged(boolean isEnabled) {
        if (!mIsVisible || !mOverlayPanel.isShowing()) return;

        if (isEnabled) {
            boolean wasMandatory = mIsMandatory;
            // Set mandatory state to false right now because it controls whether the Content
            // can be displayed. See {@link ContextualSearchPanel#canDisplayContentInPanel}.
            // Now that the feature is enable, the host will try to show the Contents.
            // See {@link ContextualSearchPanel#getContextualSearchPromoHost}.
            mIsMandatory = false;
            mHost.onPromoOptIn(wasMandatory);
        } else {
            mHost.onPromoOptOut();
        }

        collapse();
    }

    /**
     * @return Whether the Promo is visible.
     */
    public boolean isVisible() {
        return mIsVisible;
    }

    /**
     * @return Whether the Promo is mandatory.
     */
    public boolean isMandatory() {
        return mIsMandatory;
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

    // ============================================================================================
    // Promo Acceptance Animation
    // ============================================================================================

    /**
     * Collapses the Promo in an animated fashion.
     */
    public void collapse() {
        hidePromoView();

        CompositorAnimator collapse =
                CompositorAnimator.ofFloat(mOverlayPanel.getAnimationHandler(), 1.f, 0.f,
                        OverlayPanelAnimation.BASE_ANIMATION_DURATION_MS, null);

        collapse.addUpdateListener(animator -> updateAppearance(animator.getAnimatedValue()));

        collapse.addListener(new AnimatorListenerAdapter() {
            @Override
            public void onAnimationEnd(Animator animation) {
                hide();
            }
        });

        collapse.start();
    }

    /**
     * Updates the appearance of the Promo.
     *
     * @param percentage The completion percentage. 0.f means the Promo is fully collapsed and
     *                   transparent. 1.f means the Promo is fully expanded and opaque.
     */
    private void updateAppearance(float percentage) {
        if (mIsVisible) {
            mHeightPx = Math.round(MathUtils.clamp(percentage * mContentHeightPx,
                    0.f, mContentHeightPx));
            mOpacity = percentage;
        } else {
            mHeightPx = 0.f;
            mOpacity = 0.f;
        }

        mHost.onUpdatePromoAppearance();
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
            calculatePromoHeight();
        }
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();

        View view = getView();

        // "Allow" button.
        Button allowButton = (Button) view.findViewById(R.id.contextual_search_allow_button);
        allowButton.setOnClickListener(
                v -> ContextualSearchPromoControl.this.handlePromoChoice(true));

        // "No thanks" button.
        Button noThanksButton = (Button) view.findViewById(R.id.contextual_search_no_thanks_button);
        noThanksButton.setOnClickListener(
                v -> ContextualSearchPromoControl.this.handlePromoChoice(false));

        // Fill in text with link to Settings.
        TextView promoText = (TextView) view.findViewById(R.id.contextual_search_promo_text);

        NoUnderlineClickableSpan settingsLink = new NoUnderlineClickableSpan(
                (View ignored) -> ContextualSearchPromoControl.this.handleClickSettingsLink());

        promoText.setText(SpanApplier.applySpans(
                view.getResources().getString(R.string.contextual_search_short_description),
                new SpanApplier.SpanInfo("<link>", "</link>", settingsLink)));
        promoText.setMovementMethod(LinkMovementMethod.getInstance());

        calculatePromoHeight();
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
     * @param hasEnabled Whether the user has chosen to enable the feature.
     */
    private void handlePromoChoice(boolean hasEnabled) {
        if (!mHasHandledChoice) {
            mHasHandledChoice = true;
            PrefServiceBridge.getInstance().setContextualSearchState(hasEnabled);
        }
    }

    /**
     * Handles a click in the settings link located in the Promo.
     */
    private void handleClickSettingsLink() {
        new Handler().post(new Runnable() {
            @Override
            public void run() {
                PreferencesLauncher.launchSettingsPage(getContext(),
                        ContextualSearchPreferenceFragment.class.getName());
            }
        });
    }

    // ============================================================================================
    // Helpers
    // ============================================================================================

    /**
     * Shows the Promo Android View. By making the Android View visible, we are allowing the
     * Promo to be interactive. Since snapshots are not interactive (they are just a bitmap),
     * we need to temporarily show the Android View on top of the snapshot, so the user will
     * be able to click in the Promo buttons and/or link.
     */
    private void showPromoView() {
        float y = getYPx();
        View view = getView();
        if (view == null
                || !mIsVisible
                || (mIsShowingView && mPromoViewY == y)
                || mHeightPx == 0.f) return;

        float offsetX = mOverlayPanel.getOffsetX() * mDpToPx;
        if (LocalizationUtils.isLayoutRtl()) {
            offsetX = -offsetX;
        }

        view.setTranslationX(offsetX);
        view.setTranslationY(y);
        view.setVisibility(View.VISIBLE);

        // NOTE(pedrosimonetti): We need to call requestLayout, otherwise
        // the Promo View will not become visible.
        view.requestLayout();

        mIsShowingView = true;
        mPromoViewY = y;

        // The Promo can only be interacted when the View is being displayed.
        mWasInteractive = true;
    }

    /**
     * Hides the Promo Android View. See {@link #showPromoView()}.
     */
    private void hidePromoView() {
        View view = getView();
        if (view == null
                || !mIsVisible
                || !mIsShowingView) {
            return;
        }

        view.setVisibility(View.INVISIBLE);

        mIsShowingView = false;
    }

    /**
     * @return The current Y position of the Promo.
     */
    private float getYPx() {
        return Math.round(
                (mOverlayPanel.getOffsetY() + mOverlayPanel.getBarContainerHeight()) * mDpToPx);
    }

    /**
     * Calculates the content height of the Promo View, and adjusts the height of the Promo while
     * preserving the proportion of the height with the content height. This should be called
     * whenever the the size of the Promo View changes.
     */
    private void calculatePromoHeight() {
        layout();

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