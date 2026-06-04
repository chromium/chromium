// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp_customization.theme_sync;

import static org.chromium.chrome.browser.ntp_customization.theme_sync.NtpThemeSyncHistoryProperties.HIGHLIGHTED_ITEM_INDEX;
import static org.chromium.chrome.browser.ntp_customization.theme_sync.NtpThemeSyncHistoryProperties.IS_VISIBLE;
import static org.chromium.chrome.browser.ntp_customization.theme_sync.NtpThemeSyncHistoryProperties.MORE_OPTIONS_CLICK_LISTENER;
import static org.chromium.chrome.browser.ntp_customization.theme_sync.NtpThemeSyncHistoryProperties.RECYCLER_VIEW_ADAPTER;
import static org.chromium.chrome.browser.ntp_customization.theme_sync.NtpThemeSyncHistoryProperties.RECYCLER_VIEW_LAYOUT_MANAGER;

import android.view.View;

import androidx.recyclerview.widget.RecyclerView;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.ntp_customization.R;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.widget.ButtonCompat;

/** View binder class for the NTP theme sync history. */
@NullMarked
public class NtpThemeSyncHistoryContainerViewBinder {
    public static void bind(PropertyModel model, View view, PropertyKey propertyKey) {
        RecyclerView recyclerView = view.findViewById(R.id.ntp_theme_sync_history_recycler_view);

        if (propertyKey == RECYCLER_VIEW_ADAPTER) {
            recyclerView.setAdapter(model.get(RECYCLER_VIEW_ADAPTER));
        } else if (propertyKey == RECYCLER_VIEW_LAYOUT_MANAGER) {
            recyclerView.setLayoutManager(model.get(RECYCLER_VIEW_LAYOUT_MANAGER));
        } else if (propertyKey == HIGHLIGHTED_ITEM_INDEX) {
            RecyclerView.Adapter adapter = recyclerView.getAdapter();
            if (adapter instanceof NtpThemeSyncHistoryRecyclerViewAdaptor recyclerViewAdaptor) {
                recyclerViewAdaptor.setSelectedPosition(
                        model.get(HIGHLIGHTED_ITEM_INDEX), /* isFromClick= */ false);
            }
        } else if (propertyKey == IS_VISIBLE) {
            view.setVisibility(model.get(IS_VISIBLE) ? View.VISIBLE : View.GONE);
        } else if (propertyKey == MORE_OPTIONS_CLICK_LISTENER) {
            ButtonCompat moreOptionsTitle = view.findViewById(R.id.more_options_title);
            moreOptionsTitle.setOnClickListener(model.get(MORE_OPTIONS_CLICK_LISTENER));
        }
    }
}
