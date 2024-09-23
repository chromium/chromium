// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.keyboard_accessory.bar_component;

import android.content.Context;
import android.view.View;

import androidx.annotation.Nullable;
import androidx.annotation.StringRes;

import org.chromium.chrome.browser.keyboard_accessory.R;
import org.chromium.chrome.browser.util.ChromeAccessibilityUtil;
import org.chromium.components.browser_ui.widget.textbubble.TextBubble;
import org.chromium.components.feature_engagement.EventConstants;
import org.chromium.components.feature_engagement.FeatureConstants;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.components.feature_engagement.TriggerState;
import org.chromium.ui.widget.RectProvider;
import org.chromium.ui.widget.ViewRectProvider;

/**
 * This class is a collection of helper functions that are used to coordinate the IPH use in the
 * keyboard accessory bar. It sets up and triggers relevant IPH libraries and is not supposed to
 * keep any state or perform any logic.
 */
class KeyboardAccessoryIPHUtils {
    /**
     * Emits a filling event that matches the given feature. Noop if no tracker is available yet.
     *
     * @param tracker The {@link Tracker} associated with the current session.
     * @param feature The feature to emit a filling event for. Fails if no event to emit.
     */
    static void emitFillingEvent(Tracker tracker, String feature) {
        if (!tracker.isInitialized()) return;
        switch (feature) {
            case FeatureConstants.KEYBOARD_ACCESSORY_ADDRESS_FILL_FEATURE:
                tracker.notifyEvent(EventConstants.KEYBOARD_ACCESSORY_ADDRESS_AUTOFILLED);
                return;
            case FeatureConstants.KEYBOARD_ACCESSORY_PASSWORD_FILLING_FEATURE:
                tracker.notifyEvent(EventConstants.KEYBOARD_ACCESSORY_PASSWORD_AUTOFILLED);
                return;
            case FeatureConstants.KEYBOARD_ACCESSORY_PAYMENT_FILLING_FEATURE:
            case FeatureConstants.KEYBOARD_ACCESSORY_PAYMENT_OFFER_FEATURE:
            case FeatureConstants.KEYBOARD_ACCESSORY_PAYMENT_VIRTUAL_CARD_DISABLED_FEATURE:
            case FeatureConstants.KEYBOARD_ACCESSORY_PAYMENT_VIRTUAL_CARD_FEATURE:
            case FeatureConstants.KEYBOARD_ACCESSORY_VIRTUAL_CARD_CVC_FILL_FEATURE:
                tracker.notifyEvent(EventConstants.KEYBOARD_ACCESSORY_PAYMENT_AUTOFILLED);
                return;
            case FeatureConstants.KEYBOARD_ACCESSORY_EXTERNAL_ACCOUNT_PROFILE_FEATURE:
                // Noop as the event is triggered in native AutofillKeyboardAccessoryControllerImpl.
                return;
            case FeatureConstants.KEYBOARD_ACCESSORY_PLUS_ADDRESS_CREATE_SUGGESTION:
                tracker.notifyEvent(
                        EventConstants.KEYBOARD_ACCESSORY_PLUS_ADDRESS_CREATE_SUGGESTION);
                return;
        }
        assert false : "No filling event emitted for feature: " + feature;
    }

    /**
     * Emits a scrolling event recording user's familiarity. Noop if no tracker is available yet.
     *
     * @param tracker The {@link Tracker} associated with the current session.
     */
    static void emitScrollingEvent(Tracker tracker) {
        if (!tracker.isInitialized()) return;
        tracker.notifyEvent(EventConstants.KEYBOARD_ACCESSORY_BAR_SWIPED);
    }

    /**
     * Used to check that filling IPH has priority over IPH that only supports filling, like the IPH
     * promoting the swipeability of the suggestions.
     *
     * @param tracker The {@link Tracker} associated with the current session.
     * @return True iff any IPH prompting to use a chip was shown before.
     */
    static boolean hasShownAnyAutofillIphBefore(Tracker tracker) {
        if (!tracker.isInitialized()) return false;
        return tracker.getTriggerState(FeatureConstants.KEYBOARD_ACCESSORY_ADDRESS_FILL_FEATURE)
                        == TriggerState.HAS_BEEN_DISPLAYED
                || tracker.getTriggerState(
                                FeatureConstants.KEYBOARD_ACCESSORY_PASSWORD_FILLING_FEATURE)
                        == TriggerState.HAS_BEEN_DISPLAYED
                || tracker.getTriggerState(
                                FeatureConstants.KEYBOARD_ACCESSORY_PAYMENT_FILLING_FEATURE)
                        == TriggerState.HAS_BEEN_DISPLAYED;
    }

    /**
     * Shows a help bubble pointing to the given rect. It contains an appropriate text for the given
     * feature. The help bubble will not be shown if the {@link Tracker} doesn't allow it anymore.
     * This may happen for example: if it was shown too often, too many IPH were triggered this
     * session or other config restrictions apply.
     *
     * @param tracker The {@link Tracker} associated with the current session.
     * @param feature A String identifying the IPH feature and its appropriate help text.
     * @param rectProvider The {@link RectProvider} providing bounds to which the bubble will point.
     * @param context Context to draw resources from.
     * @param rootView The {@link View} used to determine the maximal dimensions for the bubble.
     * @return The boolean value indicating whether the IPH has been shown.
     */
    static boolean showHelpBubble(
            Tracker tracker,
            String feature,
            RectProvider rectProvider,
            Context context,
            View rootView) {
        TextBubble helpBubble =
                createBubble(tracker, feature, rectProvider, context, rootView, null);
        if (helpBubble != null) helpBubble.show();
        return helpBubble != null;
    }

    /**
     * Shows a help bubble pointing to the given view. It contains an appropriate text for the given
     * feature. The help bubble will not be shown if the {@link Tracker} doesn't allow it anymore.
     * This may happen for example: if it was shown too often, too many IPH were triggered this
     * session or other config restrictions apply.
     *
     * @param tracker The {@link Tracker} associated with the current session.
     * @param feature A String identifying the IPH feature and its appropriate help text.
     * @param rectProvider The {@link RectProvider} providing bounds to which the bubble will point.
     * @param context Context to draw resources from.
     * @param rootView The {@link View} used to determine the maximal dimensions for the bubble.
     * @param helpText String that should be displayed within the IPH bubble.
     * @return The boolean value indicating whether the IPH has been shown.
     */
    static boolean showHelpBubble(
            Tracker tracker,
            String feature,
            RectProvider rectProvider,
            Context context,
            View rootView,
            @Nullable String helpText) {
        TextBubble helpBubble =
                createBubble(tracker, feature, rectProvider, context, rootView, helpText);
        if (helpBubble != null) helpBubble.show();
        return helpBubble != null;
    }

    /**
     * Shows a help bubble pointing to the given view. It contains an appropriate text for the given
     * feature. The help bubble will not be shown if the {@link Tracker} doesn't allow it anymore.
     * This may happen for example: if it was shown too often, too many IPH were triggered this
     * session or other config restrictions apply.
     *
     * @param tracker The {@link Tracker} associated with the current session.
     * @param feature A String identifying the IPH feature and its appropriate help text.
     * @param view The {@link View} providing context and the Rect to which the bubble will point.
     * @param rootView The {@link View} used to determine the maximal dimensions for the bubble.
     * @param helpText String that should be displayed within the IPH bubble.
     * @return The boolean value indicating whether the IPH has been shown.
     */
    static boolean showHelpBubble(
            Tracker tracker, String feature, View view, View rootView, @Nullable String helpText) {
        TextBubble helpBubble =
                createBubble(
                        tracker,
                        feature,
                        new ViewRectProvider(view),
                        view.getContext(),
                        rootView,
                        helpText);
        if (helpBubble == null) return false;
        // To emphasize which chip is pointed to, set selected to true for the built-in highlight.
        // Prefer ViewHighlighter for views without a LayerDrawable background.
        view.setSelected(true);
        helpBubble.addOnDismissListener(
                () -> {
                    view.setSelected(false);
                });
        helpBubble.show();
        return true;
    }

    private static TextBubble createBubble(
            Tracker tracker,
            String feature,
            RectProvider rectProvider,
            Context context,
            View rootView,
            @Nullable String helpText) {
        if (!tracker.isInitialized()) return null;
        if (!tracker.shouldTriggerHelpUI(feature)) return null; // This call records the IPH intent.
        TextBubble helpBubble;
        // If the help text is provided, then use it directly to generate the text bubble.
        if (helpText != null && !helpText.isEmpty()) {
            helpBubble =
                    new TextBubble(
                            context,
                            rootView,
                            helpText,
                            helpText,
                            /* showArrow= */ true,
                            rectProvider,
                            ChromeAccessibilityUtil.get().isAccessibilityEnabled());
        } else {
            @StringRes int helpTextResourceId = getHelpTextForFeature(feature);
            helpBubble =
                    new TextBubble(
                            context,
                            rootView,
                            helpTextResourceId,
                            helpTextResourceId,
                            rectProvider,
                            ChromeAccessibilityUtil.get().isAccessibilityEnabled());
        }
        helpBubble.setDismissOnTouchInteraction(true);
        helpBubble.addOnDismissListener(
                () -> {
                    tracker.dismissed(feature);
                });
        return helpBubble;
    }

    /**
     * Returns an appropriate help text for the given feature or crashes if there is none.
     *
     * @param feature A String identifying the feature.
     * @return The translated help text for the user education element.
     */
    private static @StringRes int getHelpTextForFeature(@FeatureConstants String feature) {
        switch (feature) {
            case FeatureConstants.KEYBOARD_ACCESSORY_ADDRESS_FILL_FEATURE:
            case FeatureConstants.KEYBOARD_ACCESSORY_PASSWORD_FILLING_FEATURE:
            case FeatureConstants.KEYBOARD_ACCESSORY_PAYMENT_FILLING_FEATURE:
                return R.string.iph_keyboard_accessory_fill_with_chrome;
            case FeatureConstants.KEYBOARD_ACCESSORY_BAR_SWIPING_FEATURE:
                return R.string.iph_keyboard_accessory_swipe_for_more;
            case FeatureConstants.KEYBOARD_ACCESSORY_PAYMENT_VIRTUAL_CARD_FEATURE:
                return R.string.iph_keyboard_accessory_payment_virtual_cards;
            case FeatureConstants.KEYBOARD_ACCESSORY_PAYMENT_VIRTUAL_CARD_DISABLED_FEATURE:
                return R.string.iph_keyboard_accessory_payment_virtual_cards_disabled;
            case FeatureConstants.KEYBOARD_ACCESSORY_PAYMENT_OFFER_FEATURE:
                return R.string.iph_keyboard_accessory_payment_offer;
            case FeatureConstants.KEYBOARD_ACCESSORY_EXTERNAL_ACCOUNT_PROFILE_FEATURE:
                return org.chromium.chrome.R.string
                        .autofill_iph_external_account_profile_suggestion;
            case FeatureConstants.KEYBOARD_ACCESSORY_VIRTUAL_CARD_CVC_FILL_FEATURE:
                return R.string.iph_keyboard_accessory_virtual_card_cvc_fill_feature;
        }
        assert false : "Unknown help text for feature: " + feature;
        return 0;
    }

    private KeyboardAccessoryIPHUtils() {}
}
