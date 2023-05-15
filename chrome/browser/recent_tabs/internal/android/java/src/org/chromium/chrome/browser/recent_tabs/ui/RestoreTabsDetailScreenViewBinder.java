// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.recent_tabs.ui;

import static org.chromium.chrome.browser.recent_tabs.RestoreTabsProperties.CURRENT_SCREEN;
import static org.chromium.chrome.browser.recent_tabs.RestoreTabsProperties.DETAIL_SCREEN_BACK_CLICK_HANDLER;
import static org.chromium.chrome.browser.recent_tabs.RestoreTabsProperties.DETAIL_SCREEN_MODEL_LIST;
import static org.chromium.chrome.browser.recent_tabs.RestoreTabsProperties.DETAIL_SCREEN_TITLE;
import static org.chromium.chrome.browser.recent_tabs.RestoreTabsProperties.ScreenType.DEVICE_SCREEN;

import android.view.View;
import android.widget.ImageButton;
import android.widget.TextView;

import androidx.recyclerview.widget.RecyclerView;

import org.chromium.chrome.browser.recent_tabs.R;
import org.chromium.chrome.browser.recent_tabs.RestoreTabsProperties.DetailItemType;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.SimpleRecyclerViewAdapter;

/**
 * This class is responsible for pushing updates to the Restore Tabs detail screen view. These
 * updates are pulled from the RestoreTabsProperties when a notification of an update is
 * received.
 */
public class RestoreTabsDetailScreenViewBinder {
    static class ViewHolder {
        final View mContentView;

        ViewHolder(View contentView) {
            mContentView = contentView;
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
            if (currentScreen == DEVICE_SCREEN) {
                RestoreTabsViewBinderHelper.allKeysBinder(
                        model, view, RestoreTabsDetailScreenViewBinder::bindDeviceScreen);
            }
        } else if (currentScreen == DEVICE_SCREEN) {
            bindDeviceScreen(model, view, propertyKey);
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
            adapter.registerType(DetailItemType.DEVICE, ForeignSessionItemViewBinder::create,
                    ForeignSessionItemViewBinder::bind);
            view.setAdapter(adapter, view);
        }
    }

    private static void bindCommonProperties(
            PropertyModel model, ViewHolder view, PropertyKey propertyKey) {
        if (propertyKey == DETAIL_SCREEN_BACK_CLICK_HANDLER) {
            getToolbarBackImageButton(view).setOnClickListener(
                    (v) -> model.get(DETAIL_SCREEN_BACK_CLICK_HANDLER).run());
        } else if (propertyKey == DETAIL_SCREEN_TITLE) {
            String titleText = view.mContentView.getContext().getResources().getString(
                    model.get(DETAIL_SCREEN_TITLE));
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
}
