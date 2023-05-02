// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.recent_tabs.ui;

import static org.chromium.chrome.browser.recent_tabs.RestoreTabsProperties.ALL_KEYS;
import static org.chromium.chrome.browser.recent_tabs.RestoreTabsProperties.CURRENT_SCREEN;
import static org.chromium.chrome.browser.recent_tabs.RestoreTabsProperties.HOME_SCREEN_DELEGATE;
import static org.chromium.chrome.browser.recent_tabs.RestoreTabsProperties.NUM_TABS_DESELECTED;
import static org.chromium.chrome.browser.recent_tabs.RestoreTabsProperties.REVIEW_TABS_MODEL_LIST;
import static org.chromium.chrome.browser.recent_tabs.RestoreTabsProperties.SELECTED_DEVICE;
import static org.chromium.chrome.browser.recent_tabs.RestoreTabsProperties.ScreenType.HOME_SCREEN;
import static org.chromium.chrome.browser.recent_tabs.RestoreTabsProperties.ScreenType.UNINITIALIZED;

import android.text.format.DateUtils;
import android.view.View;
import android.widget.LinearLayout;
import android.widget.TextView;

import org.chromium.chrome.browser.recent_tabs.ForeignSessionHelper.ForeignSession;
import org.chromium.chrome.browser.recent_tabs.R;
import org.chromium.chrome.browser.recent_tabs.ui.RestoreTabsPromoScreenCoordinator.Delegate;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.widget.ButtonCompat;

/**
 * This class is responsible for pushing updates to the Restore Tabs promo screen view. These
 * updates are pulled from the RestoreTabsproperties when a notification of an update is
 * received.
 */
public class RestoreTabsPromoScreenViewBinder {
    /**
     * A functional interface to perform a callback and run screen specific bind logic.
     */
    interface BindScreenCallback {
        /**
         * Perform bind logic on all property keys for the respective screen.
         */
        void bind(PropertyModel model, ViewHolder view, PropertyKey propertyKey);
    }

    static class ViewHolder {
        final View mContentView;

        ViewHolder(View contentView) {
            mContentView = contentView;
        }
    }

    public static void bind(PropertyModel model, ViewHolder view, PropertyKey propertyKey) {
        int currentScreen = model.get(CURRENT_SCREEN);

        if (propertyKey == CURRENT_SCREEN) {
            switch (currentScreen) {
                case HOME_SCREEN:
                    allKeysBinder(model, view, RestoreTabsPromoScreenViewBinder::bindHomeScreen);
                    break;
                default:
                    assert currentScreen == UNINITIALIZED : "Switching to an unidentified screen.";
            }
        } else {
            switch (currentScreen) {
                case HOME_SCREEN:
                    bindHomeScreen(model, view, propertyKey);
                    break;
                default:
                    assert currentScreen == UNINITIALIZED : "Unidentified current screen.";
            }
        }
    }

    private static void bindHomeScreen(
            PropertyModel model, ViewHolder view, PropertyKey propertyKey) {
        if (propertyKey == HOME_SCREEN_DELEGATE) {
            Delegate delegate = model.get(HOME_SCREEN_DELEGATE);

            getSelectedDeviceView(view).setOnClickListener((v) -> delegate.onShowDeviceList());

            int numSelectedTabs =
                    model.get(REVIEW_TABS_MODEL_LIST).size() - model.get(NUM_TABS_DESELECTED);
            getRestoreTabsButton(view).setEnabled(numSelectedTabs != 0);
            getRestoreTabsButton(view).setText(
                    view.mContentView.getContext().getResources().getQuantityString(
                            R.plurals.restore_tabs_promo_sheet_restore_tabs, numSelectedTabs,
                            numSelectedTabs));
            getRestoreTabsButton(view).setOnClickListener((v) -> {
                getRestoreTabsButton(view).announceForAccessibility(
                        view.mContentView.getContext().getResources().getString(
                                R.string.restore_tabs_promo_sheet_open_tabs_button_clicked_description));
                delegate.onAllTabsChosen();
            });

            getReviewTabsButton(view).setOnClickListener((v) -> {
                getReviewTabsButton(view).announceForAccessibility(
                        view.mContentView.getContext().getResources().getString(
                                R.string.restore_tabs_promo_sheet_review_tabs_button_clicked_description));
                delegate.onReviewTabsChosen();
            });
        } else if (propertyKey == SELECTED_DEVICE) {
            updateDevice(model, view);
        }
    }

    private static void updateDevice(PropertyModel model, ViewHolder view) {
        ForeignSession session = model.get(SELECTED_DEVICE);
        if (session == null) {
            return;
        }

        getDeviceNameTextView(view).setText(session.name);
        CharSequence lastModifiedTimeString = DateUtils.getRelativeTimeSpanString(
                session.modifiedTime, System.currentTimeMillis(), 0);
        String sessionInfo = view.mContentView.getContext().getResources().getQuantityString(
                R.plurals.restore_tabs_promo_sheet_device_info,
                model.get(REVIEW_TABS_MODEL_LIST).size(),
                Integer.toString(model.get(REVIEW_TABS_MODEL_LIST).size()), lastModifiedTimeString);
        getSessionInfoTextView(view).setText(sessionInfo);
    }

    private static void allKeysBinder(
            PropertyModel model, ViewHolder view, BindScreenCallback callback) {
        for (PropertyKey propertyKey : ALL_KEYS) {
            callback.bind(model, view, propertyKey);
        }
    }

    private static TextView getDeviceNameTextView(ViewHolder view) {
        return view.mContentView.findViewById(R.id.restore_tabs_promo_sheet_device_name);
    }

    private static TextView getSessionInfoTextView(ViewHolder view) {
        return view.mContentView.findViewById(R.id.restore_tabs_promo_sheet_session_info);
    }

    private static LinearLayout getSelectedDeviceView(ViewHolder view) {
        return view.mContentView.findViewById(R.id.selected_device_view);
    }

    private static ButtonCompat getRestoreTabsButton(ViewHolder view) {
        return view.mContentView.findViewById(R.id.restore_tabs_button_open_tabs);
    }

    private static ButtonCompat getReviewTabsButton(ViewHolder view) {
        return view.mContentView.findViewById(R.id.restore_tabs_button_review_tabs);
    }
}
