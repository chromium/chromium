// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.selection;

import android.content.pm.ResolveInfo;

import androidx.annotation.IntDef;
import androidx.annotation.NonNull;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.components.autofill.AutofillSelectionActionMenuDelegate;
import org.chromium.content_public.browser.SelectionMenuItem;
import org.chromium.content_public.browser.SelectionPopupController;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.ArrayList;
import java.util.List;

/**
 * The WebView delegate customizing text selection menu items in {@link SelectionPopupController}.
 * It records webview-specific histograms and ensures the order of Samsung-specific menu entries.
 */
public class AwSelectionActionMenuDelegate extends AutofillSelectionActionMenuDelegate {
    private static final String TEXT_SELECTION_MENU_ORDERING_HISTOGRAM_NAME =
            "Android.WebView.TextSelectionMenuOrdering";

    // This should be kept in sync with the definition
    // |AndroidWebViewTextSelectionMenuOrdering| in
    // tools/metrics/histograms/metadata/android/enums.xml
    @IntDef({
        TextSelectionMenuOrdering.DEFAULT_MENU_ORDER,
        TextSelectionMenuOrdering.SAMSUNG_MENU_ORDER,
        TextSelectionMenuOrdering.COUNT
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface TextSelectionMenuOrdering {
        int DEFAULT_MENU_ORDER = 0;
        int SAMSUNG_MENU_ORDER = 1;
        int COUNT = 2;
    }

    public AwSelectionActionMenuDelegate() {
        if (SamsungSelectionActionMenuHelper.shouldUseSamsungMenuItemOrdering()) {
            RecordHistogram.recordEnumeratedHistogram(
                    TEXT_SELECTION_MENU_ORDERING_HISTOGRAM_NAME,
                    TextSelectionMenuOrdering.SAMSUNG_MENU_ORDER,
                    TextSelectionMenuOrdering.COUNT);
        } else {
            RecordHistogram.recordEnumeratedHistogram(
                    TEXT_SELECTION_MENU_ORDERING_HISTOGRAM_NAME,
                    TextSelectionMenuOrdering.DEFAULT_MENU_ORDER,
                    TextSelectionMenuOrdering.COUNT);
        }
    }

    @Override
    public void modifyDefaultMenuItems(
            List<SelectionMenuItem.Builder> menuItemBuilders,
            boolean isSelectionPassword,
            @NonNull String selectedText) {
        if (SamsungSelectionActionMenuHelper.shouldUseSamsungMenuItemOrdering()) {
            SamsungSelectionActionMenuHelper.modifyDefaultMenuItems(
                    menuItemBuilders, isSelectionPassword, selectedText);
        }
    }

    @Override
    public List<ResolveInfo> filterTextProcessingActivities(List<ResolveInfo> activities) {
        if (SamsungSelectionActionMenuHelper.isManageAppsSupported()) {
            return SamsungSelectionActionMenuHelper.filterTextProcessingActivities(activities);
        }
        return activities;
    }

    @NonNull
    @Override
    public List<SelectionMenuItem> getAdditionalTextProcessingItems() {
        if (SamsungSelectionActionMenuHelper.isManageAppsSupported()) {
            return SamsungSelectionActionMenuHelper.getAdditionalTextProcessingItems();
        }
        return new ArrayList<>();
    }
}
