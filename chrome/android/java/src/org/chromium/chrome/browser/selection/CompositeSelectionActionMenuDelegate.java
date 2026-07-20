// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.selection;

import android.content.Context;
import android.content.pm.ResolveInfo;
import android.view.View;

import org.chromium.base.SelectionActionMenuClientWrapper.DefaultItem;
import org.chromium.base.SelectionActionMenuClientWrapper.MenuType;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.content_public.browser.SelectionMenuItem;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.selection.SelectionActionMenuDelegate;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

/**
 * A composite {@link SelectionActionMenuDelegate} that delegates to a list of underlying
 * SelectionActionMenuDelegate delegates.
 */
@NullMarked
public class CompositeSelectionActionMenuDelegate implements SelectionActionMenuDelegate {
    private final List<SelectionActionMenuDelegate> mDelegates = new ArrayList<>();

    public void addDelegate(SelectionActionMenuDelegate delegate) {
        mDelegates.add(delegate);
    }

    @Override
    public @DefaultItem int[] getDefaultMenuItemOrder(@MenuType int menuType) {
        @DefaultItem int[] defaultOrder = SelectionActionMenuDelegate.getDefaultMenuItemOrder();
        for (SelectionActionMenuDelegate delegate : mDelegates) {
            @DefaultItem int[] order = delegate.getDefaultMenuItemOrder(menuType);
            if (!Arrays.equals(order, defaultOrder)) {
                return order;
            }
        }
        return defaultOrder;
    }

    /** Filter activities by every delegates, and then return what're remaining. */
    @Override
    public List<ResolveInfo> filterTextProcessingActivities(
            @MenuType int menuType, List<ResolveInfo> activities) {
        List<ResolveInfo> filteredActivities = activities;
        for (SelectionActionMenuDelegate delegate : mDelegates) {
            filteredActivities =
                    delegate.filterTextProcessingActivities(menuType, filteredActivities);
        }
        return filteredActivities;
    }

    @Override
    public boolean canReuseCachedSelectionMenu(@MenuType int menuType) {
        for (SelectionActionMenuDelegate delegate : mDelegates) {
            if (!delegate.canReuseCachedSelectionMenu(menuType)) {
                return false;
            }
        }
        return true;
    }

    @Override
    public List<SelectionMenuItem> getAdditionalMenuItems(
            @MenuType int menuType,
            boolean isSelectionPassword,
            boolean isSelectionReadOnly,
            String selectedText) {
        List<SelectionMenuItem> items = new ArrayList<>();
        for (SelectionActionMenuDelegate delegate : mDelegates) {
            items.addAll(
                    delegate.getAdditionalMenuItems(
                            menuType, isSelectionPassword, isSelectionReadOnly, selectedText));
        }
        return items;
    }

    @Override
    public boolean handleMenuItemClick(
            SelectionMenuItem item, WebContents webContents, @Nullable View containerView) {
        for (SelectionActionMenuDelegate delegate : mDelegates) {
            if (delegate.handleMenuItemClick(item, webContents, containerView)) {
                return true;
            }
        }
        return false;
    }

    @Override
    public @Nullable String getWebSearchMenuItemTitle(Context context, String selectedText) {
        String title = null;
        for (SelectionActionMenuDelegate delegate : mDelegates) {
            title = delegate.getWebSearchMenuItemTitle(context, selectedText);
            if (title != null) {
                break;
            }
        }
        return title;
    }
}
