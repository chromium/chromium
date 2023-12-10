// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.recent_tabs.ui;

import static org.chromium.chrome.browser.recent_tabs.RestoreTabsProperties.CURRENT_SCREEN;
import static org.chromium.chrome.browser.recent_tabs.RestoreTabsProperties.DEVICE_MODEL_LIST;
import static org.chromium.chrome.browser.recent_tabs.RestoreTabsProperties.HOME_SCREEN_DELEGATE;
import static org.chromium.chrome.browser.recent_tabs.RestoreTabsProperties.NUM_TABS_DESELECTED;
import static org.chromium.chrome.browser.recent_tabs.RestoreTabsProperties.REVIEW_TABS_MODEL_LIST;
import static org.chromium.chrome.browser.recent_tabs.RestoreTabsProperties.SELECTED_DEVICE;
import static org.chromium.chrome.browser.recent_tabs.RestoreTabsProperties.ScreenType.HOME_SCREEN;

import android.text.format.DateUtils;
import android.view.View;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.TextView;

import org.chromium.chrome.browser.recent_tabs.ForeignSessionHelper.ForeignSession;
import org.chromium.chrome.browser.recent_tabs.R;
import org.chromium.chrome.browser.recent_tabs.ui.RestoreTabsPromoScreenCoordinator.Delegate;
import org.chromium.components.sync_device_info.FormFactor;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.widget.ButtonCompat;

/**
 * This class is responsible for pushing updates to the Restore Tabs promo screen view. These
 * updates are pulled from the RestoreTabsProperties when a notification of an update is
 * received.
 */
public class RestoreTabsPromoScreenViewBinder {
    static class ViewHolder {
        final View mContentView;

        ViewHolder(View contentView) {
            mContentView = contentView;
        }
    }

    // This binder handles logic that targets when the CURRENT_SCREEN switches to HOME_SCREEN.
    public static void bind(PropertyModel model, ViewHolder view, PropertyKey propertyKey) {
        int currentScreen = model.get(CURRENT_SCREEN);

        if (propertyKey == CURRENT_SCREEN) {
            if (currentScreen == HOME_SCREEN) {
                RestoreTabsViewBinderHelper.allKeysBinder(
                        model, view, RestoreTabsPromoScreenViewBinder::bindHomeScreen);
            }
        } else if (currentScreen == HOME_SCREEN) {
            bindHomeScreen(model, view, propertyKey);
        }
    }

    private static void bindHomeScreen(
            PropertyModel model, ViewHolder view, PropertyKey propertyKey) {
        if (propertyKey == HOME_SCREEN_DELEGATE) {
            var resources = view.mContentView.getContext().getResources();
            Delegate delegate = model.get(HOME_SCREEN_DELEGATE);

            int numDevices = model.get(DEVICE_MODEL_LIST).size();
            if (numDevices != 1) {
                getExpandIconSelectorView(view)
                        .setImageResource(R.drawable.restore_tabs_expand_more);
                getSelectedDeviceView(view).setOnClickListener((v) -> delegate.onShowDeviceList());
                getSheetSubtitleTextView(view)
                        .setText(
                                resources.getString(
                                        R.string.restore_tabs_promo_sheet_subtitle_multi_device));
            } else {
                getExpandIconSelectorView(view).setVisibility(View.GONE);
                getSelectedDeviceView(view).setOnClickListener(null);
                getSheetSubtitleTextView(view)
                        .setText(
                                resources.getString(
                                        R.string.restore_tabs_promo_sheet_subtitle_single_device));
            }

            int numSelectedTabs =
                    model.get(REVIEW_TABS_MODEL_LIST).size() - model.get(NUM_TABS_DESELECTED);
            getRestoreTabsButton(view).setEnabled(numSelectedTabs != 0);
            getRestoreTabsButton(view)
                    .setText(
                            resources.getQuantityString(
                                    R.plurals.restore_tabs_open_tabs,
                                    numSelectedTabs,
                                    numSelectedTabs));
            getRestoreTabsButton(view)
                    .setOnClickListener(
                            (v) -> {
                                var context = view.mContentView.getContext();
                                String restoreDescription =
                                        context.getString(
                                                R.string
                                                        .restore_tabs_open_tabs_button_clicked_description);
                                getRestoreTabsButton(view)
                                        .announceForAccessibility(restoreDescription);
                                delegate.onAllTabsChosen();
                            });

            getReviewTabsButton(view)
                    .setOnClickListener(
                            (v) -> {
                                var context = view.mContentView.getContext();
                                String reviewDescription =
                                        context.getString(
                                                R.string
                                                        .restore_tabs_promo_sheet_review_tabs_button_clicked_description);
                                getReviewTabsButton(view)
                                        .announceForAccessibility(reviewDescription);
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

        if (session.formFactor == FormFactor.PHONE) {
            getDeviceIconView(view).setImageResource(R.drawable.restore_tabs_phone_icon);
        } else if (session.formFactor == FormFactor.TABLET) {
            getDeviceIconView(view).setImageResource(R.drawable.restore_tabs_tablet_icon);
        }

        getDeviceNameTextView(view).setText(session.name);
        CharSequence lastModifiedTimeString =
                DateUtils.getRelativeTimeSpanString(
                        session.modifiedTime, System.currentTimeMillis(), 0);
        String sessionInfo =
                view.mContentView
                        .getContext()
                        .getResources()
                        .getQuantityString(
                                R.plurals.restore_tabs_promo_sheet_device_info,
                                model.get(REVIEW_TABS_MODEL_LIST).size(),
                                Integer.toString(model.get(REVIEW_TABS_MODEL_LIST).size()),
                                lastModifiedTimeString);
        getSessionInfoTextView(view).setText(sessionInfo);
    }

    private static TextView getDeviceNameTextView(ViewHolder view) {
        return view.mContentView.findViewById(R.id.restore_tabs_promo_sheet_device_name);
    }

    private static TextView getSessionInfoTextView(ViewHolder view) {
        return view.mContentView.findViewById(R.id.restore_tabs_promo_sheet_session_info);
    }

    private static LinearLayout getSelectedDeviceView(ViewHolder view) {
        return view.mContentView.findViewById(R.id.restore_tabs_selected_device_view);
    }

    private static ButtonCompat getRestoreTabsButton(ViewHolder view) {
        return view.mContentView.findViewById(R.id.restore_tabs_button_open_tabs);
    }

    private static ButtonCompat getReviewTabsButton(ViewHolder view) {
        return view.mContentView.findViewById(R.id.restore_tabs_button_review_tabs);
    }

    private static ImageView getExpandIconSelectorView(ViewHolder view) {
        return view.mContentView.findViewById(R.id.restore_tabs_expand_icon_device_selection);
    }

    private static ImageView getDeviceIconView(ViewHolder view) {
        return view.mContentView.findViewById(R.id.restore_tabs_promo_sheet_device_icon);
    }

    private static TextView getSheetSubtitleTextView(ViewHolder view) {
        return view.mContentView.findViewById(R.id.restore_tabs_promo_sheet_subtitle);
    }
}
