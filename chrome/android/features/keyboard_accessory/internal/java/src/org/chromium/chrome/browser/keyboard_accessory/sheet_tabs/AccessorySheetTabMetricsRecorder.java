// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.keyboard_accessory.sheet_tabs;

import static org.chromium.chrome.browser.keyboard_accessory.ManualFillingMetricsRecorder.getHistogramForType;
import static org.chromium.chrome.browser.keyboard_accessory.sheet_tabs.AccessorySheetTabModel.AccessorySheetDataPiece.Type.ADDRESS_INFO;
import static org.chromium.chrome.browser.keyboard_accessory.sheet_tabs.AccessorySheetTabModel.AccessorySheetDataPiece.Type.CREDIT_CARD_INFO;
import static org.chromium.chrome.browser.keyboard_accessory.sheet_tabs.AccessorySheetTabModel.AccessorySheetDataPiece.Type.PASSWORD_INFO;
import static org.chromium.chrome.browser.keyboard_accessory.sheet_tabs.AccessorySheetTabModel.AccessorySheetDataPiece.getType;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.browser.keyboard_accessory.AccessoryTabType;
import org.chromium.chrome.browser.keyboard_accessory.data.KeyboardAccessoryData.UserInfo;
import org.chromium.chrome.browser.keyboard_accessory.data.UserInfoField;
import org.chromium.ui.modelutil.ListModel;

/**
 * This class provides helpers to record metrics related to a specific accessory sheet tab.
 */
class AccessorySheetTabMetricsRecorder {
    static final String UMA_KEYBOARD_ACCESSORY_SHEET_SUGGESTIONS =
            "KeyboardAccessory.AccessorySheetSuggestionCount";

    /**
     * The Recorder itself should be stateless and have no need for an instance.
     */
    private AccessorySheetTabMetricsRecorder() {}

    /**
     * Records the number of interactive suggestions in the given list.
     * @param tabType The tab that contained the list.
     * @param suggestionList The list containing all suggestions.
     */
    static void recordSheetSuggestions(@AccessoryTabType int tabType,
            ListModel<AccessorySheetTabModel.AccessorySheetDataPiece> suggestionList) {
        int interactiveSuggestions = 0;
        for (int i = 0; i < suggestionList.size(); ++i) {
            if (getType(suggestionList.get(i)) == PASSWORD_INFO
                    || getType(suggestionList.get(i)) == ADDRESS_INFO
                    || getType(suggestionList.get(i)) == CREDIT_CARD_INFO) {
                UserInfo info = (UserInfo) suggestionList.get(i).getDataPiece();
                for (UserInfoField field : info.getFields()) {
                    if (field.isSelectable()) ++interactiveSuggestions;
                }
            }
        }
        RecordHistogram.recordCount100Histogram(
                getHistogramForType(UMA_KEYBOARD_ACCESSORY_SHEET_SUGGESTIONS, tabType),
                interactiveSuggestions);
        if (tabType != AccessoryTabType.ALL) { // Record count for all tab types exactly once!
            RecordHistogram.recordCount100Histogram(
                    getHistogramForType(
                            UMA_KEYBOARD_ACCESSORY_SHEET_SUGGESTIONS, AccessoryTabType.ALL),
                    interactiveSuggestions);
        }
    }
}
