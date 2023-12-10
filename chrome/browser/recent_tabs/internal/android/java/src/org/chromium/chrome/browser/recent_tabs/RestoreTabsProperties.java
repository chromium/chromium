// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.recent_tabs;

import androidx.annotation.IntDef;

import org.chromium.chrome.browser.recent_tabs.ForeignSessionHelper.ForeignSession;
import org.chromium.chrome.browser.recent_tabs.ui.RestoreTabsDetailScreenCoordinator;
import org.chromium.chrome.browser.recent_tabs.ui.RestoreTabsPromoScreenCoordinator;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModel.ReadableObjectPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableBooleanPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableIntPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/** State for the Restore Tabs promo UI. */
public class RestoreTabsProperties {
    /** The different screens that can be shown on the sheet. */
    @IntDef({
        ScreenType.UNINITIALIZED,
        ScreenType.HOME_SCREEN,
        ScreenType.DEVICE_SCREEN,
        ScreenType.REVIEW_TABS_SCREEN
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface ScreenType {
        int UNINITIALIZED = 0;
        int HOME_SCREEN = 1;
        int DEVICE_SCREEN = 2;
        int REVIEW_TABS_SCREEN = 3;
    }

    /** The different item types in the RecyclerView on the detail bottom sheet. */
    @IntDef({DetailItemType.DEVICE, DetailItemType.TAB})
    @Retention(RetentionPolicy.SOURCE)
    public @interface DetailItemType {
        /** A device entry. */
        int DEVICE = 1;

        /** A tab entry. */
        int TAB = 2;
    }

    /** Property that indicates the bottom sheet visibility. */
    public static final WritableBooleanPropertyKey VISIBLE = new WritableBooleanPropertyKey();

    /** Indicates which ScreenType is currently displayed on the bottom sheet. */
    public static final WritableIntPropertyKey CURRENT_SCREEN = new WritableIntPropertyKey();

    /** The chosen device for restoring tabs from. */
    public static final WritableObjectPropertyKey<ForeignSession> SELECTED_DEVICE =
            new WritableObjectPropertyKey<>();

    /** The models corresponding to all user synced devices. */
    public static final ReadableObjectPropertyKey<ModelList> DEVICE_MODEL_LIST =
            new ReadableObjectPropertyKey();

    /** The models corresponding to all synced tabs from the selected device. */
    public static final ReadableObjectPropertyKey<ModelList> REVIEW_TABS_MODEL_LIST =
            new ReadableObjectPropertyKey();

    /**
     * The delegate that handles actions on the home screen.
     * This key is effectively read-only, as it is set once in the RestoreTabsMediator.
     */
    public static final WritableObjectPropertyKey<RestoreTabsPromoScreenCoordinator.Delegate>
            HOME_SCREEN_DELEGATE = new WritableObjectPropertyKey<>();

    /** The string id of the title shown on the detail screen. */
    public static final WritableIntPropertyKey DETAIL_SCREEN_TITLE = new WritableIntPropertyKey();

    /** The handler for the back icon on the detail screens (device select, review tabs). */
    public static final WritableObjectPropertyKey<Runnable> DETAIL_SCREEN_BACK_CLICK_HANDLER =
            new WritableObjectPropertyKey<>();

    /** The delegate that handles actions on the review tabs screen. */
    public static final WritableObjectPropertyKey<RestoreTabsDetailScreenCoordinator.Delegate>
            REVIEW_TABS_SCREEN_DELEGATE = new WritableObjectPropertyKey<>();

    /**
     * The models that are displayed on the detail screen. This will either point to
     * DEVICE_MODEL_LIST or REVIEW_TABS_MODEL_LIST.
     */
    public static final WritableObjectPropertyKey<ModelList> DETAIL_SCREEN_MODEL_LIST =
            new WritableObjectPropertyKey<>();

    /** The number of tabs deselected on the review tabs screen. */
    public static final WritableIntPropertyKey NUM_TABS_DESELECTED = new WritableIntPropertyKey();

    public static PropertyModel createDefaultModel() {
        return new PropertyModel.Builder(ALL_KEYS)
                .with(VISIBLE, false)
                .with(CURRENT_SCREEN, ScreenType.UNINITIALIZED)
                .with(DEVICE_MODEL_LIST, new ModelList())
                .with(REVIEW_TABS_MODEL_LIST, new ModelList())
                .with(NUM_TABS_DESELECTED, 0)
                .build();
    }

    /** All keys used for the restore tabs promo bottom sheet. */
    public static final PropertyKey[] ALL_KEYS =
            new PropertyKey[] {
                VISIBLE,
                CURRENT_SCREEN,
                SELECTED_DEVICE,
                DEVICE_MODEL_LIST,
                REVIEW_TABS_MODEL_LIST,
                HOME_SCREEN_DELEGATE,
                DETAIL_SCREEN_TITLE,
                DETAIL_SCREEN_BACK_CLICK_HANDLER,
                REVIEW_TABS_SCREEN_DELEGATE,
                DETAIL_SCREEN_MODEL_LIST,
                NUM_TABS_DESELECTED
            };
}
