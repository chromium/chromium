// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp.cards.promo;

import android.content.Context;
import android.content.res.ColorStateList;
import android.content.res.Resources;
import android.graphics.drawable.Drawable;
import android.view.View;

import androidx.annotation.Nullable;
import androidx.appcompat.content.res.AppCompatResources;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.homepage.HomepageManager;
import org.chromium.chrome.browser.homepage.HomepageManager.HomepageStateListener;
import org.chromium.chrome.browser.ntp.cards.promo.HomepagePromoUtils.HomepagePromoAction;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.components.browser_ui.widget.promo.PromoCardCoordinator;
import org.chromium.components.browser_ui.widget.promo.PromoCardCoordinator.LayoutStyle;
import org.chromium.components.browser_ui.widget.promo.PromoCardProperties;
import org.chromium.components.feature_engagement.EventConstants;
import org.chromium.components.feature_engagement.FeatureConstants;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * Controller for the homepage promo component, managing the creation of homepage promo.
 * The logic for creating and managing the promo is relatively simple, so this class merges the duty
 * of mediator and coordinator.
 *
 * TODO(wenyufu): Consider making the promo controller an {@link
 * org.chromium.chrome.browser.ntp.cards.OptionalLeaf} similar to {@link
 * org.chromium.chrome.browser.ntp.cards.SignInPromo}.
 */
public class HomepagePromoController implements HomepageStateListener {
    /**
     * Interface that represents the holder of HomepagePromo. When the promo has state changes due
     * to homepage changes / being dismissed, {@link #onPromoDataChange()} will be called.
     */
    public interface HomepagePromoStateListener {
        /**
         * Called when promo has state changes due to homepage changes / being dismissed.
         */
        void onHomepagePromoStateChange();
    }

    private final Context mContext;
    private final HomepagePromoSnackbarController mSnackbarController;
    private final Tracker mTracker;

    // Created only when creation criteria is met.
    private HomepagePromoStateListener mStateListener;
    private PromoCardCoordinator mPromoCoordinator;
    private PropertyModel mModel;

    private boolean mIsPromoShowing;

    /**
     * Build the HomepagePromoController that handles the set up / tear down for the homepage promo.
     * @param context Context from the activity.
     * @param snackbarManager SnackbarManager used to display snackbar.
     * @param tracker Tracker for the feature engagement system.
     */
    public HomepagePromoController(
            Context context, SnackbarManager snackbarManager, Tracker tracker) {
        mContext = context;
        mSnackbarController = new HomepagePromoSnackbarController(context, snackbarManager);

        // Inform event of creation.
        mTracker = tracker;
    }

    /**
     * @param listener Listener to be notified when the promo should be removed from the parent
     *         view.
     */
    public void setHomepagePromoStateListener(@Nullable HomepagePromoStateListener listener) {
        mStateListener = listener;
    }

    /**
     * Retrieve the view representing HomepagePromo.
     *
     * Internally, this function will check the creation criteria from SharedPreference and feature
     * engagement system. If the creation criteria is not met, this function will return null;
     * otherwise, promo controller will create the view lazily if not yet created, and return.
     *
     * Note that the same View will be returned until the promo is dismissed by internal or external
     * signal.
     *
     * @return View represent HomepagePromo if creation criteria is meet; null If the criteria is
     *         not meet.
     */
    public @Nullable View getPromoView() {
        if (mIsPromoShowing || HomepagePromoUtils.shouldCreatePromo(mTracker)) {
            mIsPromoShowing = true;
            return getPromoCoordinator().getView();
        } else {
            return null;
        }
    }

    /**
     * Destroy the HomepagePromo and release its dependencies.
     */
    public void destroy() {
        mStateListener = null;

        // Early return if promo coordinator is not initialized.
        if (mPromoCoordinator == null) return;

        mPromoCoordinator.destroy();
        HomepageManager.getInstance().removeListener(this);

        // Update the tracker if the promo is shown and not yet dismissed
        if (mIsPromoShowing) dismissPromoInternal();
    }

    /**
     * @return The PromoCardCoordinator for homepage promo. If the coordinator is not created,
     *         create it lazily.
     */
    private PromoCardCoordinator getPromoCoordinator() {
        if (mPromoCoordinator != null) return mPromoCoordinator;

        @LayoutStyle
        int layoutStyle = HomepagePromoVariationManager.getInstance().getLayoutVariation();
        mModel = buildModel(layoutStyle);

        mPromoCoordinator = new PromoCardCoordinator(
                mContext, mModel, FeatureConstants.HOMEPAGE_PROMO_CARD_FEATURE, layoutStyle);

        mPromoCoordinator.getView().setId(R.id.homepage_promo);

        // Subscribe to homepage update only when view is created.
        HomepageManager.getInstance().addListener(this);

        HomepagePromoUtils.recordHomepagePromoEvent(HomepagePromoAction.CREATED);

        return mPromoCoordinator;
    }

    private PropertyModel buildModel(@LayoutStyle int layoutStyle) {
        Resources r = mContext.getResources();

        PropertyModel.Builder builder = new PropertyModel.Builder(PromoCardProperties.ALL_KEYS);

        builder.with(PromoCardProperties.PRIMARY_BUTTON_CALLBACK, (v) -> onPrimaryButtonClicked())
                .with(PromoCardProperties.IMPRESSION_SEEN_CALLBACK, this::onPromoSeen)
                .with(PromoCardProperties.IS_IMPRESSION_ON_PRIMARY_BUTTON, true);

        if (layoutStyle == LayoutStyle.SLIM) {
            Drawable homeIcon = AppCompatResources.getDrawable(
                    mContext, org.chromium.chrome.browser.toolbar.R.drawable.btn_toolbar_home);
            ColorStateList tint =
                    AppCompatResources.getColorStateList(mContext, R.color.default_icon_color_blue);

            builder.with(PromoCardProperties.IMAGE, homeIcon)
                    .with(PromoCardProperties.ICON_TINT, tint)
                    .with(PromoCardProperties.TITLE,
                            r.getString(R.string.homepage_promo_title_slim))
                    .with(PromoCardProperties.PRIMARY_BUTTON_TEXT,
                            r.getString(R.string.homepage_promo_primary_button_slim));

        } else if (layoutStyle == LayoutStyle.LARGE) {
            Drawable illustration = AppCompatResources.getDrawable(
                    mContext, R.drawable.homepage_promo_illustration_vector);

            builder.with(PromoCardProperties.IMAGE, illustration)
                    .with(PromoCardProperties.TITLE, r.getString(R.string.homepage_promo_title))
                    .with(PromoCardProperties.DESCRIPTION,
                            r.getString(R.string.homepage_promo_description))
                    .with(PromoCardProperties.PRIMARY_BUTTON_TEXT,
                            r.getString(R.string.homepage_promo_primary_button))
                    .with(PromoCardProperties.HAS_SECONDARY_BUTTON, true)
                    .with(PromoCardProperties.SECONDARY_BUTTON_TEXT,
                            r.getString(R.string.no_thanks))
                    .with(PromoCardProperties.SECONDARY_BUTTON_CALLBACK, (v) -> dismissPromo());

        } else { // layoutStyle == LayoutStyle.COMPACT
            Drawable homeIcon = AppCompatResources.getDrawable(
                    mContext, org.chromium.chrome.browser.toolbar.R.drawable.btn_toolbar_home);
            ColorStateList tint =
                    AppCompatResources.getColorStateList(mContext, R.color.default_icon_color_blue);

            builder.with(PromoCardProperties.IMAGE, homeIcon)
                    .with(PromoCardProperties.ICON_TINT, tint)
                    .with(PromoCardProperties.TITLE, r.getString(R.string.homepage_promo_title))
                    .with(PromoCardProperties.DESCRIPTION,
                            r.getString(R.string.homepage_promo_description))
                    .with(PromoCardProperties.PRIMARY_BUTTON_TEXT,
                            r.getString(R.string.homepage_promo_primary_button))
                    .with(PromoCardProperties.HAS_SECONDARY_BUTTON, true)
                    .with(PromoCardProperties.SECONDARY_BUTTON_TEXT,
                            r.getString(R.string.no_thanks))
                    .with(PromoCardProperties.SECONDARY_BUTTON_CALLBACK, (v) -> dismissPromo());
        }

        return builder.build();
    }

    /**
     * Dismissed the promo and record the user action.
     */
    public void dismissPromo() {
        HomepagePromoUtils.setPromoDismissedInSharedPreference(true);
        HomepagePromoUtils.recordHomepagePromoEvent(HomepagePromoAction.DISMISSED);
        dismissPromoInternal();
    }

    private void dismissPromoInternal() {
        mIsPromoShowing = false;
        mTracker.dismissed(FeatureConstants.HOMEPAGE_PROMO_CARD_FEATURE);
        if (mStateListener != null) mStateListener.onHomepagePromoStateChange();
    }

    private void onPrimaryButtonClicked() {
        HomepageManager manager = HomepageManager.getInstance();
        boolean wasUsingNtp = manager.getPrefHomepageUseChromeNTP();
        boolean wasUsingDefaultUri = manager.getPrefHomepageUseDefaultUri();
        String originalCustomUri = manager.getPrefHomepageCustomUri();

        mTracker.notifyEvent(EventConstants.HOMEPAGE_PROMO_ACCEPTED);
        HomepagePromoUtils.recordHomepagePromoEvent(HomepagePromoAction.ACCEPTED);

        manager.setHomepagePreferences(true, false, originalCustomUri);
        mSnackbarController.showUndoSnackbar(wasUsingNtp, wasUsingDefaultUri, originalCustomUri);
    }

    private void onPromoSeen() {
        mTracker.notifyEvent(EventConstants.HOMEPAGE_PROMO_SEEN);
        HomepagePromoUtils.recordHomepagePromoEvent(HomepagePromoAction.SEEN);
    }

    /**
     * When the homepage is no longer using default NTP, update the visibility of promo.
     */
    @Override
    public void onHomepageStateUpdated() {
        if (mIsPromoShowing && !HomepagePromoUtils.shouldCreatePromo(null)) {
            dismissPromoInternal();
        }
    }
}
