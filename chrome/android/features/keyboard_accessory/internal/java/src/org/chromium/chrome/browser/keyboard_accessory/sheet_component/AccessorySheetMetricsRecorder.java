// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.keyboard_accessory.sheet_component;

import static org.chromium.chrome.browser.keyboard_accessory.AccessorySheetTrigger.MANUAL_OPEN;
import static org.chromium.chrome.browser.keyboard_accessory.ManualFillingMetricsRecorder.recordSheetTrigger;
import static org.chromium.chrome.browser.keyboard_accessory.sheet_component.AccessorySheetProperties.ACTIVE_TAB_INDEX;
import static org.chromium.chrome.browser.keyboard_accessory.sheet_component.AccessorySheetProperties.TABS;
import static org.chromium.chrome.browser.keyboard_accessory.sheet_component.AccessorySheetProperties.VISIBLE;

import org.chromium.chrome.browser.keyboard_accessory.AccessorySheetTrigger;
import org.chromium.chrome.browser.keyboard_accessory.AccessoryTabType;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * This class provides helpers to record general metrics about accessory sheets.
 * It sets up an observers to observe {@link AccessorySheetProperties}-based models and records
 * metrics accordingly.
 */
class AccessorySheetMetricsRecorder {
    /**
     * The Recorder itself should be stateless and have no need for an instance.
     */
    private AccessorySheetMetricsRecorder() {}

    /**
     * Registers an observer to the given model that records changes for all properties.
     * @param accessorySheetModel The observable {@link AccessorySheetProperties}.
     */
    static void registerAccessorySheetModelMetricsObserver(PropertyModel accessorySheetModel) {
        accessorySheetModel.addObserver((source, propertyKey) -> {
            if (propertyKey == VISIBLE) {
                if (accessorySheetModel.get(VISIBLE)) {
                    int activeTab = accessorySheetModel.get(ACTIVE_TAB_INDEX);
                    if (activeTab >= 0 && activeTab < accessorySheetModel.get(TABS).size()) {
                        recordSheetTrigger(
                                accessorySheetModel.get(TABS).get(activeTab).getRecordingType(),
                                MANUAL_OPEN);
                    }
                } else {
                    recordSheetTrigger(AccessoryTabType.ALL, AccessorySheetTrigger.ANY_CLOSE);
                }
                return;
            }
            if (propertyKey == ACTIVE_TAB_INDEX || propertyKey == AccessorySheetProperties.HEIGHT
                    || propertyKey == AccessorySheetProperties.TOP_SHADOW_VISIBLE
                    || propertyKey == AccessorySheetProperties.PAGE_CHANGE_LISTENER) {
                return;
            }
            assert false : "Every property update needs to be handled explicitly!";
        });
    }
}
