// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.keyboard_accessory.bar_component;

import android.view.View;

import androidx.annotation.StringRes;

import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.keyboard_accessory.R;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.ui.widget.textbubble.ImageTextBubble;
import org.chromium.components.feature_engagement.EventConstants;
import org.chromium.components.feature_engagement.FeatureConstants;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.ui.widget.ChipView;
import org.chromium.ui.widget.ViewRectProvider;

/**
 * This class is a collection of helper functions that are used to coordinate the IPH use in the
 * keyboard accessory bar. It sets up and triggers relevant IPH libraries and is not supposed to
 * keep any state or perform any logic.
 */
class KeyboardAccessoryIPHUtils {
    /**
     * Emits a filling event that matches the given feature. Noop if no tracker is available yet.
     * @param feature The feature to emit a filling event for. Fails if no event to emit.
     */
    static void emitFillingEvent(String feature) {
        final Tracker tracker = TrackerFactory.getTrackerForProfile(Profile.getLastUsedProfile());
        if (!tracker.isInitialized()) return;
        switch (feature) {
            case FeatureConstants.KEYBOARD_ACCESSORY_ADDRESS_FILL_FEATURE:
                tracker.notifyEvent(EventConstants.KEYBOARD_ACCESSORY_ADDRESS_AUTOFILLED);
                return;
            case FeatureConstants.KEYBOARD_ACCESSORY_PASSWORD_FILLING_FEATURE:
                tracker.notifyEvent(EventConstants.KEYBOARD_ACCESSORY_PASSWORD_AUTOFILLED);
                return;
            case FeatureConstants.KEYBOARD_ACCESSORY_PAYMENT_FILLING_FEATURE:
                tracker.notifyEvent(EventConstants.KEYBOARD_ACCESSORY_PAYMENT_AUTOFILLED);
                return;
        }
        assert false : "No event emitted for feature: " + feature;
    }

    /**
     * Shows a help bubble pointing to the given view. It contains an appropriate text for the given
     * feature. The help bubble will not be shown if the {@link Tracker} doesn't allow it anymore.
     * This may happen for example: if it was shown too often, too many IPH were triggered this
     * session or other config restrictions apply.
     * @param feature A String identifying the IPH feature and its appropriate help text.
     * @param view The {@link View} providing context and the Rect to which the bubble will point.
     * @param rootView The {@link View} used to determine the maximal dimensions for the bubble.
     */
    static void showHelpBubble(String feature, ChipView view, View rootView) {
        final Tracker tracker = TrackerFactory.getTrackerForProfile(Profile.getLastUsedProfile());
        if (!tracker.isInitialized()) return;
        if (!tracker.shouldTriggerHelpUI(feature)) return; // This call records the IPH intent.
        @StringRes
        int helpText = getHelpTextForFeature(feature);
        ImageTextBubble helpBubble = new ImageTextBubble(view.getContext(), rootView, helpText,
                helpText, true, new ViewRectProvider(view), R.drawable.ic_chrome);
        helpBubble.setDismissOnTouchInteraction(true);
        helpBubble.show();
        // To emphasize which chip is pointed to, set selected to true for the built-in highlight.
        // Prefer ViewHighlighter for views without a LayerDrawable background.
        view.setSelected(true);
        helpBubble.addOnDismissListener(() -> {
            tracker.dismissed(feature);
            view.setSelected(false);
        });
    }

    /**
     * Returns an appropriate help text for the given feature or crashes if there is none.
     * @param feature A String identifying the feature.
     * @return The translated help text for the user education element.
     */
    private static @StringRes int getHelpTextForFeature(@FeatureConstants String feature) {
        switch (feature) {
            case FeatureConstants.KEYBOARD_ACCESSORY_ADDRESS_FILL_FEATURE:
                return R.string.iph_keyboard_accessory_fill_address;
            case FeatureConstants.KEYBOARD_ACCESSORY_PASSWORD_FILLING_FEATURE:
                return R.string.iph_keyboard_accessory_fill_password;
            case FeatureConstants.KEYBOARD_ACCESSORY_PAYMENT_FILLING_FEATURE:
                return R.string.iph_keyboard_accessory_fill_payment;
        }
        assert false : "Unknown help text for feature: " + feature;
        return 0;
    }

    private KeyboardAccessoryIPHUtils() {}
}
