// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.recent_tabs;

import androidx.annotation.IntDef;

import org.chromium.chrome.browser.recent_tabs.ForeignSessionHelper.ForeignSession;
import org.chromium.chrome.browser.recent_tabs.ui.RestoreTabsPromoScreenCoordinator;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModel.WritableBooleanPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableIntPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * State for the Restore Tabs promo UI.
 */
public class RestoreTabsProperties {
    /**
     * The different screens that can be shown on the sheet.
     */
    @IntDef({ScreenType.HOME_SCREEN, ScreenType.DEVICE_SCREEN, ScreenType.REVIEW_TABS_SCREEN})
    @Retention(RetentionPolicy.SOURCE)
    public @interface ScreenType {
        int HOME_SCREEN = 0;
        int DEVICE_SCREEN = 1;
        int REVIEW_TABS_SCREEN = 2;
    }

    /** Property that indicates the bottom sheet visibility. */
    public static final WritableBooleanPropertyKey VISIBLE = new WritableBooleanPropertyKey();

    /** Indicates which ScreenType is currently displayed on the bottom sheet. */
    public static final WritableIntPropertyKey CURRENT_SCREEN = new WritableIntPropertyKey();

    /** The chosen device for restoring tabs from. */
    public static final WritableObjectPropertyKey<ForeignSession> SELECTED_DEVICE =
            new WritableObjectPropertyKey<>();

    /** The delegate that handles actions on the home screen. */
    public static final WritableObjectPropertyKey<RestoreTabsPromoScreenCoordinator.Delegate>
            HOME_SCREEN_DELEGATE = new WritableObjectPropertyKey<>();

    public static PropertyModel createDefaultModel() {
        return new PropertyModel.Builder(ALL_KEYS)
                .with(VISIBLE, false)
                .with(CURRENT_SCREEN, ScreenType.HOME_SCREEN)
                .build();
    }

    /** All keys used for the restore tabs promo bottom sheet. */
    static final PropertyKey[] ALL_KEYS =
            new PropertyKey[] {VISIBLE, CURRENT_SCREEN, SELECTED_DEVICE, HOME_SCREEN_DELEGATE};
}
