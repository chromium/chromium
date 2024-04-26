// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.keyboard_accessory;

import org.chromium.base.metrics.RecordHistogram;

import java.security.InvalidParameterException;

/**
 * This class provides helpers to record metrics related to the keyboard accessory and its sheets.
 */
public class ManualFillingMetricsRecorder {
    public static final String UMA_KEYBOARD_ACCESSORY_ACTION_IMPRESSION =
            "KeyboardAccessory.AccessoryActionImpression";
    public static final String UMA_KEYBOARD_ACCESSORY_ACTION_SELECTED =
            "KeyboardAccessory.AccessoryActionSelected";
    public static final String UMA_KEYBOARD_ACCESSORY_TOGGLE_IMPRESSION =
            "KeyboardAccessory.AccessoryToggleImpression";
    public static final String UMA_KEYBOARD_ACCESSORY_TOGGLE_CLICKED =
            "KeyboardAccessory.AccessoryToggleClicked";
    public static final String UMA_KEYBOARD_ACCESSORY_SHEET_TRIGGERED =
            "KeyboardAccessory.AccessorySheetTriggered";
    private static final String UMA_KEYBOARD_ACCESSORY_SHEET_SUGGESTION_SELECTED =
            "KeyboardAccessory.AccessorySheetSuggestionsSelected";
    private static final String UMA_KEYBOARD_ACCESSORY_TOUCH_EVENT_FILTERED =
            "KeyboardAccessory.TouchEventFiltered";
    private static final String UMA_KEYBOARD_ACCESSORY_SHEET_TYPE_SUFFIX_PASSWORDS = "Passwords";
    private static final String UMA_KEYBOARD_ACCESSORY_SHEET_TYPE_SUFFIX_CREDIT_CARDS =
            "CreditCards";
    private static final String UMA_KEYBOARD_ACCESSORY_SHEET_TYPE_SUFFIX_ADDRESSES = "Addresses";

    /** The Recorder itself should be stateless and have no need for an instance. */
    private ManualFillingMetricsRecorder() {}

    /**
     * Gets the complete name of a histogram for the given tab type.
     * @param baseHistogram the base histogram.
     * @param tabType The tab type that determines the histogram's suffix.
     * @return The complete name of the histogram.
     */
    public static String getHistogramForType(String baseHistogram, @AccessoryTabType int tabType) {
        switch (tabType) {
            case AccessoryTabType.ALL:
                return baseHistogram;
            case AccessoryTabType.PASSWORDS:
                return baseHistogram + "." + UMA_KEYBOARD_ACCESSORY_SHEET_TYPE_SUFFIX_PASSWORDS;
            case AccessoryTabType.CREDIT_CARDS:
                return baseHistogram + "." + UMA_KEYBOARD_ACCESSORY_SHEET_TYPE_SUFFIX_CREDIT_CARDS;
            case AccessoryTabType.ADDRESSES:
                return baseHistogram + "." + UMA_KEYBOARD_ACCESSORY_SHEET_TYPE_SUFFIX_ADDRESSES;
            case AccessoryTabType.OBSOLETE_TOUCH_TO_FILL:
                assert false : "Obsolete histogram: " + tabType;
        }
        assert false : "Undefined histogram for tab type " + tabType + " !";
        return "";
    }

    /**
     * Records why an accessory sheet was toggled.
     * @param tabType The tab that was selected to trigger the sheet.
     * @param bucket The {@link AccessorySheetTrigger} to record.
     */
    public static void recordSheetTrigger(
            @AccessoryTabType int tabType, @AccessorySheetTrigger int bucket) {
        RecordHistogram.recordEnumeratedHistogram(
                getHistogramForType(UMA_KEYBOARD_ACCESSORY_SHEET_TRIGGERED, tabType),
                bucket,
                AccessorySheetTrigger.COUNT);
        if (tabType != AccessoryTabType.ALL) { // Record count for all tab types exactly once!
            RecordHistogram.recordEnumeratedHistogram(
                    getHistogramForType(
                            UMA_KEYBOARD_ACCESSORY_SHEET_TRIGGERED, AccessoryTabType.ALL),
                    bucket,
                    AccessorySheetTrigger.COUNT);
        }
    }

    /**
     * Records a selected action.
     * @param bucket An {@link AccessoryAction}.
     */
    public static void recordActionSelected(@AccessoryAction int bucket) {
        RecordHistogram.recordEnumeratedHistogram(
                UMA_KEYBOARD_ACCESSORY_ACTION_SELECTED, bucket, AccessoryAction.COUNT);
    }

    /**
     * Records an impression for a single action.
     * @param bucket An {@link AccessoryAction} that was just impressed.
     */
    public static void recordActionImpression(@AccessoryAction int bucket) {
        RecordHistogram.recordEnumeratedHistogram(
                UMA_KEYBOARD_ACCESSORY_ACTION_IMPRESSION, bucket, AccessoryAction.COUNT);
    }

    /**
     * Records an impression for a single toggle.
     *
     * @param toggleType An {@link AccessoryToggleType} that was just seen. This includes the state
     * that the toggle was in.
     */
    public static void recordToggleImpression(@AccessoryToggleType int toggleType) {
        RecordHistogram.recordEnumeratedHistogram(
                UMA_KEYBOARD_ACCESSORY_TOGGLE_IMPRESSION, toggleType, AccessoryToggleType.COUNT);
    }

    /**
     * Records a click for a single toggle.
     *
     * @param toggleType An {@link AccessoryToggleType} that was just tapped. This includes the
     * state that the toggle was in before the user clicked it.
     */
    public static void recordToggleClicked(@AccessoryToggleType int toggleType) {
        RecordHistogram.recordEnumeratedHistogram(
                UMA_KEYBOARD_ACCESSORY_TOGGLE_CLICKED, toggleType, AccessoryToggleType.COUNT);
    }

    /**
     * Records that a suggestion was selected in one of the accessory tabs.
     * @param tabType The sheet to record for. An {@link AccessoryTabType}.
     * @param isFieldObfuscated denotes whether a field is obfuscated (e.g. a password field).
     */
    static void recordSuggestionSelected(@AccessoryTabType int tabType, boolean isFieldObfuscated) {
        @AccessorySuggestionType int suggestionRecordingType = AccessorySuggestionType.COUNT;
        switch (tabType) {
            case AccessoryTabType.PASSWORDS:
                suggestionRecordingType =
                        isFieldObfuscated
                                ? AccessorySuggestionType.PASSWORD
                                : AccessorySuggestionType.USERNAME;
                break;
            case AccessoryTabType.CREDIT_CARDS:
                suggestionRecordingType = AccessorySuggestionType.PAYMENT_INFO;
                break;
            case AccessoryTabType.ADDRESSES:
                // TODO(crbug.com/41460210): Consider splitting and/or separate recording.
                suggestionRecordingType = AccessorySuggestionType.ADDRESS_INFO;
                break;
            case AccessoryTabType.OBSOLETE_TOUCH_TO_FILL:
                throw new InvalidParameterException("Obsolete tabType: " + tabType);
            case AccessoryTabType.ALL:
                throw new InvalidParameterException("Unable to handle tabType: " + tabType);
        }

        // TODO(crbug.com/41460210): Double-check we don't record twice with new address filling.
        RecordHistogram.recordEnumeratedHistogram(
                getHistogramForType(
                        UMA_KEYBOARD_ACCESSORY_SHEET_SUGGESTION_SELECTED, AccessoryTabType.ALL),
                suggestionRecordingType,
                AccessorySuggestionType.COUNT);
        RecordHistogram.recordEnumeratedHistogram(
                getHistogramForType(UMA_KEYBOARD_ACCESSORY_SHEET_SUGGESTION_SELECTED, tabType),
                suggestionRecordingType,
                AccessorySuggestionType.COUNT);
    }

    /**
     * Records if at least 1 motion event was filtered because the Chrome window was fully or
     * partially obscured.
     *
     * @param hasFilteredTouchEvents if at least 1 touch event was filtered by the {@link
     *     KeyboardAccessoryView}.
     */
    public static void recordHasFilteredTouchEvents(boolean hasFilteredTouchEvents) {
        RecordHistogram.recordBooleanHistogram(
                UMA_KEYBOARD_ACCESSORY_TOUCH_EVENT_FILTERED, hasFilteredTouchEvents);
    }
}
