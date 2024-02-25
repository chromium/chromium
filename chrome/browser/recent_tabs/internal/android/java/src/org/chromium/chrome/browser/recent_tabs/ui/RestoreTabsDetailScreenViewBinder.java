// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.recent_tabs.ui;

import static org.chromium.chrome.browser.recent_tabs.RestoreTabsProperties.CURRENT_SCREEN;
import static org.chromium.chrome.browser.recent_tabs.RestoreTabsProperties.DETAIL_SCREEN_BACK_CLICK_HANDLER;
import static org.chromium.chrome.browser.recent_tabs.RestoreTabsProperties.DETAIL_SCREEN_MODEL_LIST;
import static org.chromium.chrome.browser.recent_tabs.RestoreTabsProperties.DETAIL_SCREEN_TITLE;
import static org.chromium.chrome.browser.recent_tabs.RestoreTabsProperties.NUM_TABS_DESELECTED;
import static org.chromium.chrome.browser.recent_tabs.RestoreTabsProperties.REVIEW_TABS_MODEL_LIST;
import static org.chromium.chrome.browser.recent_tabs.RestoreTabsProperties.REVIEW_TABS_SCREEN_DELEGATE;
import static org.chromium.chrome.browser.recent_tabs.RestoreTabsProperties.ScreenType.DEVICE_SCREEN;
import static org.chromium.chrome.browser.recent_tabs.RestoreTabsProperties.ScreenType.HOME_SCREEN;
import static org.chromium.chrome.browser.recent_tabs.RestoreTabsProperties.ScreenType.REVIEW_TABS_SCREEN;
import static org.chromium.chrome.browser.recent_tabs.RestoreTabsProperties.ScreenType.UNINITIALIZED;

import android.view.View;
import android.widget.ImageButton;
import android.widget.TextView;

import androidx.recyclerview.widget.RecyclerView;

import org.chromium.chrome.browser.recent_tabs.R;
import org.chromium.chrome.browser.recent_tabs.RestoreTabsProperties.DetailItemType;
import org.chromium.chrome.browser.recent_tabs.ui.RestoreTabsDetailScreenCoordinator.Delegate;
import org.chromium.chrome.browser.recent_tabs.ui.TabItemViewBinder.BindContext;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.SimpleRecyclerViewAdapter;
import org.chromium.ui.widget.ButtonCompat;

/**
 * This class is responsible for pushing updates to the Restore Tabs detail screen view. These
 * updates are pulled from the RestoreTabsProperties when a notification of an update is
 * received.
 */
public class RestoreTabsDetailScreenViewBinder {
    static class ViewHolder {
        final View mContentView;
        final BindContext mBindContext;

        ViewHolder(View contentView, BindContext bindContext) {
            mContentView = contentView;
            mBindContext = bindContext;
        }

        public void setAdapter(RecyclerView.Adapter adapter, ViewHolder view) {
            getRecyclerView(view).setAdapter(adapter);
        }
    }

    // This binder handles logic that targets when the CURRENT_SCREEN switches to DEVICE_SCREEN or
    // REVIEW_TABS_SCREEN.
    public static void bind(PropertyModel model, ViewHolder view, PropertyKey propertyKey) {
        int currentScreen = model.get(CURRENT_SCREEN);

        if (propertyKey == CURRENT_SCREEN) {
            switch (currentScreen) {
                case DEVICE_SCREEN:
                    RestoreTabsViewBinderHelper.allKeysBinder(
                            model, view, RestoreTabsDetailScreenViewBinder::bindDeviceScreen);
                    break;
                case REVIEW_TABS_SCREEN:
                    RestoreTabsViewBinderHelper.allKeysBinder(
                            model, view, RestoreTabsDetailScreenViewBinder::bindReviewTabsScreen);
                    break;
                case HOME_SCREEN:
                    break;
                default:
                    assert currentScreen == UNINITIALIZED : "Switching to an unidentified screen.";
            }
        } else {
            switch (currentScreen) {
                case DEVICE_SCREEN:
                    bindDeviceScreen(model, view, propertyKey);
                    break;
                case REVIEW_TABS_SCREEN:
                    bindReviewTabsScreen(model, view, propertyKey);
                    break;
                case HOME_SCREEN:
                    break;
                default:
                    assert currentScreen == UNINITIALIZED : "Unidentified current screen.";
            }
        }
    }

    public static void bindDeviceScreen(
            PropertyModel model, ViewHolder view, PropertyKey propertyKey) {
        bindCommonProperties(model, view, propertyKey);

        if (propertyKey == DETAIL_SCREEN_MODEL_LIST) {
            if (model.get(DETAIL_SCREEN_MODEL_LIST) == null) {
                return;
            }

            SimpleRecyclerViewAdapter adapter =
                    new SimpleRecyclerViewAdapter(model.get(DETAIL_SCREEN_MODEL_LIST));
            adapter.registerType(
                    DetailItemType.DEVICE,
                    ForeignSessionItemViewBinder::create,
                    ForeignSessionItemViewBinder::bind);
            view.setAdapter(adapter, view);
        } else if (propertyKey == REVIEW_TABS_SCREEN_DELEGATE) {
            getChangeAllTabsSelectionStateButton(view).setVisibility(View.GONE);
            getOpenSelectedTabsButton(view).setVisibility(View.GONE);
        }
    }

    public static void bindReviewTabsScreen(
            PropertyModel model, ViewHolder view, PropertyKey propertyKey) {
        bindCommonProperties(model, view, propertyKey);
        Delegate delegate = model.get(REVIEW_TABS_SCREEN_DELEGATE);

        if (propertyKey == DETAIL_SCREEN_MODEL_LIST) {
            if (model.get(DETAIL_SCREEN_MODEL_LIST) == null) {
                return;
            }

            SimpleRecyclerViewAdapter adapter =
                    new SimpleRecyclerViewAdapter(model.get(DETAIL_SCREEN_MODEL_LIST));
            adapter.registerType(
                    DetailItemType.TAB,
                    TabItemViewBinder::create,
                    (tabModel, tabView, tabPropertyKey) -> {
                        TabItemViewBinder.bind(
                                tabModel, tabView, tabPropertyKey, view.mBindContext);
                    });
            view.setAdapter(adapter, view);
        } else if (propertyKey == REVIEW_TABS_SCREEN_DELEGATE) {
            getChangeAllTabsSelectionStateButton(view).setVisibility(View.VISIBLE);
            getOpenSelectedTabsButton(view).setVisibility(View.VISIBLE);
        } else if (propertyKey == NUM_TABS_DESELECTED) {
            getChangeAllTabsSelectionStateButton(view)
                    .setOnClickListener(
                            (v) -> {
                                getChangeAllTabsSelectionStateButton(view)
                                        .announceForAccessibility(
                                                view.mContentView
                                                        .getContext()
                                                        .getResources()
                                                        .getString(
                                                                R.string
                                                                        .restore_tabs_review_tabs_screen_change_all_tabs_selection_button_clicked_description));
                                delegate.onChangeSelectionStateForAllTabs();
                            });
            getOpenSelectedTabsButton(view)
                    .setOnClickListener(
                            (v) -> {
                                getOpenSelectedTabsButton(view)
                                        .announceForAccessibility(
                                                view.mContentView
                                                        .getContext()
                                                        .getResources()
                                                        .getString(
                                                                R.string
                                                                        .restore_tabs_open_tabs_button_clicked_description));
                                delegate.onSelectedTabsChosen();
                            });

            int numSelectedTabs =
                    model.get(REVIEW_TABS_MODEL_LIST).size() - model.get(NUM_TABS_DESELECTED);
            getOpenSelectedTabsButton(view).setEnabled(numSelectedTabs != 0);
            getOpenSelectedTabsButton(view)
                    .setText(
                            view.mContentView
                                    .getContext()
                                    .getResources()
                                    .getQuantityString(
                                            R.plurals.restore_tabs_open_tabs,
                                            numSelectedTabs,
                                            numSelectedTabs));

            int allTabsSelectionString =
                    (model.get(NUM_TABS_DESELECTED) == 0)
                            ? R.string.restore_tabs_review_tabs_screen_deselect_all
                            : R.string.restore_tabs_review_tabs_screen_select_all;
            getChangeAllTabsSelectionStateButton(view)
                    .setText(
                            view.mContentView
                                    .getContext()
                                    .getResources()
                                    .getString(allTabsSelectionString));
        }
    }

    private static void bindCommonProperties(
            PropertyModel model, ViewHolder view, PropertyKey propertyKey) {
        if (propertyKey == DETAIL_SCREEN_BACK_CLICK_HANDLER) {
            getToolbarBackImageButton(view)
                    .setOnClickListener((v) -> model.get(DETAIL_SCREEN_BACK_CLICK_HANDLER).run());
        } else if (propertyKey == DETAIL_SCREEN_TITLE) {
            String titleText =
                    view.mContentView
                            .getContext()
                            .getResources()
                            .getString(model.get(DETAIL_SCREEN_TITLE));
            getToolbarTitleTextView(view).setText(titleText);
        }
    }

    private static ImageButton getToolbarBackImageButton(ViewHolder view) {
        return view.mContentView.findViewById(R.id.restore_tabs_toolbar_back_image_button);
    }

    private static TextView getToolbarTitleTextView(ViewHolder view) {
        return view.mContentView.findViewById(R.id.restore_tabs_toolbar_title_text_view);
    }

    private static RecyclerView getRecyclerView(ViewHolder view) {
        return view.mContentView.findViewById(R.id.restore_tabs_detail_screen_recycler_view);
    }

    private static ButtonCompat getChangeAllTabsSelectionStateButton(ViewHolder view) {
        return view.mContentView.findViewById(R.id.restore_tabs_button_change_all_tabs_selection);
    }

    private static ButtonCompat getOpenSelectedTabsButton(ViewHolder view) {
        return view.mContentView.findViewById(R.id.restore_tabs_button_open_selected_tabs);
    }
}
