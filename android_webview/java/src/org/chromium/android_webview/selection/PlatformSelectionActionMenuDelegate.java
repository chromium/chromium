// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.selection;

import android.content.Context;
import android.content.pm.ResolveInfo;
import android.text.TextUtils;
import android.view.MenuItem;
import android.view.View;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.ContextUtils;
import org.chromium.base.SelectionActionMenuClientWrapper;
import org.chromium.base.SelectionActionMenuClientWrapper.DefaultItem;
import org.chromium.base.SelectionActionMenuClientWrapper.MenuType;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.autofill.AutofillSelectionActionMenuDelegate;
import org.chromium.content_public.browser.SelectionMenuItem;
import org.chromium.content_public.browser.WebContents;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;

/** SelectionActionMenuDelegate which allows the platform to customise the menu. */
@NullMarked
public class PlatformSelectionActionMenuDelegate extends AutofillSelectionActionMenuDelegate {
    private final SelectionActionMenuClientWrapper mClient;
    private final HashMap<Integer, MenuItem> mCachedItems;

    @VisibleForTesting(otherwise = VisibleForTesting.PACKAGE_PRIVATE)
    public PlatformSelectionActionMenuDelegate(SelectionActionMenuClientWrapper client) {
        mClient = client;
        mCachedItems = new HashMap<>();
    }

    @Override
    public @DefaultItem int[] getDefaultMenuItemOrder(@MenuType int menuType) {
        return mClient.getDefaultMenuItemOrder(menuType);
    }

    @Override
    public List<SelectionMenuItem> getAdditionalMenuItems(
            @MenuType int menuType,
            boolean isSelectionPassword,
            boolean isSelectionReadOnly,
            String selectedText) {
        mCachedItems.clear();
        ArrayList<SelectionMenuItem> items =
                new ArrayList<>(
                        super.getAdditionalMenuItems(
                                menuType, isSelectionPassword, isSelectionReadOnly, selectedText));
        Context context = ContextUtils.getApplicationContext();
        for (MenuItem item :
                mClient.getAdditionalMenuItems(
                        context,
                        menuType,
                        isSelectionPassword,
                        isSelectionReadOnly,
                        selectedText)) {
            if (item.getItemId() == 0 || mCachedItems.containsKey(item.getItemId())) {
                throw new RuntimeException(
                        "All menu items provided by getAdditionalMenuItems must have a unique ID.");
            }
            SelectionMenuItem menuItem = createSelectionMenuItem(item);
            if (menuItem != null) {
                items.add(menuItem);
                mCachedItems.put(item.getItemId(), item);
            }
        }
        return items;
    }

    @Override
    public List<ResolveInfo> filterTextProcessingActivities(
            @MenuType int menuType, List<ResolveInfo> activities) {
        return mClient.filterTextProcessingActivities(
                ContextUtils.getApplicationContext(), menuType, activities);
    }

    @Override
    public boolean canReuseCachedSelectionMenu(@MenuType int menuType) {
        return false;
    }

    @Override
    public boolean handleMenuItemClick(
            SelectionMenuItem item, WebContents webContents, @Nullable View containerView) {
        if (mCachedItems.containsKey(item.id)) {
            if (mClient.handleMenuItemClick(
                    ContextUtils.getApplicationContext(), mCachedItems.get(item.id))) {
                return true;
            }
        }
        return super.handleMenuItemClick(item, webContents, containerView);
    }

    private @Nullable SelectionMenuItem createSelectionMenuItem(MenuItem item) {
        if (!item.isEnabled() || !item.isVisible() || TextUtils.isEmpty(item.getTitle())) {
            return null;
        }
        return new SelectionMenuItem.Builder(item.getTitle())
                .setId(item.getItemId())
                .setGroupId(org.chromium.content.R.id.select_action_menu_delegate_items)
                .setIcon(item.getIcon())
                .setAlphabeticShortcut(item.getAlphabeticShortcut())
                .setOrder(item.getOrder())
                .setShowAsActionFlags(
                        MenuItem.SHOW_AS_ACTION_ALWAYS | MenuItem.SHOW_AS_ACTION_WITH_TEXT)
                .setContentDescription(item.getContentDescription())
                .setIntent(item.getIntent())
                .build();
    }
}
