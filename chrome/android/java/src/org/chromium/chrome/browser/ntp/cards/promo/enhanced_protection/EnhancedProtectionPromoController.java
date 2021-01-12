// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp.cards.promo.enhanced_protection;

import android.content.Context;
import android.content.res.ColorStateList;
import android.content.res.Resources;
import android.graphics.drawable.Drawable;
import android.view.View;

import androidx.annotation.Nullable;
import androidx.appcompat.content.res.AppCompatResources;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.ntp.cards.promo.enhanced_protection.EnhancedProtectionPromoUtils.EnhancedProtectionPromoAction;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.safe_browsing.metrics.SettingsAccessPoint;
import org.chromium.chrome.browser.safe_browsing.settings.SafeBrowsingSettingsFragment;
import org.chromium.chrome.browser.settings.SettingsLauncherImpl;
import org.chromium.components.browser_ui.settings.SettingsLauncher;
import org.chromium.components.browser_ui.widget.promo.PromoCardCoordinator;
import org.chromium.components.browser_ui.widget.promo.PromoCardProperties;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * Controller for the Enhanced Protection promo component.
 * The logic for creating and managing the promo is relatively simple, so this class merges the duty
 * of mediator and coordinator.
 */
public class EnhancedProtectionPromoController {
    /**
     * Interface that represents the holder of a Enhanced Protection Promo. When the promo has state
     * changes due to enhanced protection changes / being dismissed, {@link
     * #onEnhancedProtectionPromoDataChange()} will be called.
     */
    public interface EnhancedProtectionPromoStateListener {
        /**
         * Called when promo has state changes due to enhanced protection pref changes / being
         * dismissed.
         */
        void onEnhancedProtectionPromoStateChange();
    }

    private final Context mContext;
    private final @Nullable Profile mProfile;

    // Created only when creation criteria is met.
    private EnhancedProtectionPromoStateListener mStateListener;
    private PromoCardCoordinator mPromoCoordinator;
    private PropertyModel mModel;
    private boolean mIsPromoShowing;

    /**
     * Build the EnhancedProtectionPromoController that handles the set up / tear down for the
     * enhanced protection promo.
     * @param context Context from the activity.
     * @param profile Current user profile.
     */
    public EnhancedProtectionPromoController(Context context, @Nullable Profile profile) {
        mContext = context;
        mProfile = profile;
    }

    /**
     * @param listener Listener to be notified when the promo should be removed from the parent
     *         view.
     */
    public void setEnhancedProtectionPromoStateListener(
            EnhancedProtectionPromoStateListener listener) {
        mStateListener = listener;
    }

    /**
     * Retrieve the view representing EnhancedProtectionPromo.
     *
     * Internally, this function will check the creation criteria from SharedPreference. If the
     * creation criteria is not met, this function will return null; otherwise, promo controller
     * will create the view lazily if not yet created, and return.
     *
     * Note that the same View will be returned until the promo is dismissed by internal or external
     * signal.
     *
     * @return View represent EnhancedProtectionPromo if creation criteria is meet; null If the
     *         criteria is not meet.
     */
    public @Nullable View getPromoView() {
        if (mIsPromoShowing || EnhancedProtectionPromoUtils.shouldCreatePromo(mProfile)) {
            mIsPromoShowing = true;
            return getPromoCoordinator().getView();
        } else {
            return null;
        }
    }

    /**
     * Destroy the EnhancedProtectionPromo and release its dependencies.
     */
    public void destroy() {
        mStateListener = null;

        // Early return if promo coordinator is not initialized.
        if (mPromoCoordinator == null) return;

        mPromoCoordinator.destroy();

        // Update the listener if the promo is shown and not yet dismissed.
        if (mIsPromoShowing) dismissPromoInternal();
    }

    /**
     * @return The PromoCardCoordinator for enhanced protection promo. If the coordinator is not
     *         created, create it lazily.
     */
    private PromoCardCoordinator getPromoCoordinator() {
        if (mPromoCoordinator != null) return mPromoCoordinator;

        mModel = buildModel();

        mPromoCoordinator = new PromoCardCoordinator(mContext, mModel,
                EnhancedProtectionPromoUtils.ENHANCED_PROTECTION_PROMO_CARD_FEATURE);

        mPromoCoordinator.getView().setId(R.id.enhanced_protection_promo);

        EnhancedProtectionPromoUtils.recordEnhancedProtectionPromoEvent(
                EnhancedProtectionPromoAction.CREATED);

        return mPromoCoordinator;
    }

    private PropertyModel buildModel() {
        Resources r = mContext.getResources();
        Drawable securityIcon =
                AppCompatResources.getDrawable(mContext, R.drawable.ic_security_grey);
        ColorStateList tint =
                AppCompatResources.getColorStateList(mContext, R.color.default_icon_color_blue);

        PropertyModel.Builder builder = new PropertyModel.Builder(PromoCardProperties.ALL_KEYS);
        builder.with(PromoCardProperties.PRIMARY_BUTTON_CALLBACK, (v) -> onPrimaryButtonClicked())
                .with(PromoCardProperties.IMPRESSION_SEEN_CALLBACK, this::onPromoSeen)
                .with(PromoCardProperties.IS_IMPRESSION_ON_PRIMARY_BUTTON, true)
                .with(PromoCardProperties.IMAGE, securityIcon)
                .with(PromoCardProperties.ICON_TINT, tint)
                .with(PromoCardProperties.TITLE,
                        r.getString(R.string.enhanced_protection_promo_title))
                .with(PromoCardProperties.DESCRIPTION,
                        r.getString(R.string.enhanced_protection_promo_description))
                .with(PromoCardProperties.PRIMARY_BUTTON_TEXT,
                        r.getString(R.string.continue_button))
                .with(PromoCardProperties.HAS_SECONDARY_BUTTON, true)
                .with(PromoCardProperties.SECONDARY_BUTTON_TEXT, r.getString(R.string.no_thanks))
                .with(PromoCardProperties.SECONDARY_BUTTON_CALLBACK, (v) -> dismissPromo())
                .build();
        return builder.build();
    }

    /**
     * Dismissed the promo and record the user action.
     */
    private void dismissPromo() {
        EnhancedProtectionPromoUtils.setPromoDismissedInSharedPreference(true);
        EnhancedProtectionPromoUtils.recordEnhancedProtectionPromoEvent(
                EnhancedProtectionPromoAction.DISMISSED);
        dismissPromoInternal();
    }

    private void dismissPromoInternal() {
        mIsPromoShowing = false;
        if (mStateListener != null) mStateListener.onEnhancedProtectionPromoStateChange();
    }

    private void onPrimaryButtonClicked() {
        EnhancedProtectionPromoUtils.recordEnhancedProtectionPromoEvent(
                EnhancedProtectionPromoAction.ACCEPTED);
        SettingsLauncher launcher = new SettingsLauncherImpl();
        launcher.launchSettingsActivity(mContext, SafeBrowsingSettingsFragment.class,
                SafeBrowsingSettingsFragment.createArguments(
                        SettingsAccessPoint.SURFACE_EXPLORER_PROMO_SLINGER));
    }

    private void onPromoSeen() {
        EnhancedProtectionPromoUtils.recordEnhancedProtectionPromoEvent(
                EnhancedProtectionPromoAction.SEEN);
    }
}
