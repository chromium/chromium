// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.selection;

import android.view.Menu;
import android.view.MenuItem;

import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.glic.GlicKeyedService.GlicInvocationSource;
import org.chromium.chrome.browser.glic.GlicMetrics;
import org.chromium.content_public.browser.selection.SelectionDropdownMenuDelegate;
import org.chromium.ui.listmenu.ListMenuItemProperties;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.function.BooleanSupplier;

/**
 * Records Glic.WebContentsContextMenu.TextSelection for the Ask Gemini item in a text selection
 * menu. A session runs from the first time the item is shown until the menu closes and records
 * once: Executed if the item was clicked, ShownAndIgnored otherwise.
 *
 * <p>Use one instance per menu (floating toolbar, dropdown): the toolbar's close is detected a task
 * late, after a dropdown that replaced it may already be showing.
 */
@NullMarked
public class GlicSelectionMenuTracker {
    private boolean mAskGeminiShown;
    private boolean mAskGeminiClicked;

    /** Called after the floating toolbar's menu is prepared, which can happen more than once. */
    public void onFloatingMenuPrepared(Menu menu) {
        MenuItem item = menu.findItem(R.id.contextmenu_ask_gemini);
        if (item != null && item.isVisible() && item.isEnabled()) {
            mAskGeminiShown = true;
        }
    }

    /** Called when an item in the floating toolbar is clicked. */
    public void onFloatingItemClicked(int itemId) {
        if (itemId == R.id.contextmenu_ask_gemini) {
            mAskGeminiClicked = true;
        }
    }

    /**
     * Called when the floating toolbar's ActionMode is destroyed.
     *
     * @param isActionModeRunning Read after the current task, so it must check live state.
     */
    public void onFloatingMenuDestroyed(BooleanSupplier isActionModeRunning) {
        // Re-showing the menu (e.g. after a selection handle drag) destroys the ActionMode and
        // starts a new one in the same task, so the menu has only closed if no ActionMode is
        // running after this task.
        PostTask.postTask(
                TaskTraits.UI_DEFAULT,
                () -> {
                    if (!isActionModeRunning.getAsBoolean()) endSession();
                });
    }

    /** Called when the dropdown menu is shown. */
    void onDropdownMenuShown(ModelList items) {
        for (ListItem item : items) {
            if (isAskGemini(item.model)
                    && item.model.containsKey(ListMenuItemProperties.ENABLED)
                    && item.model.get(ListMenuItemProperties.ENABLED)) {
                mAskGeminiShown = true;
                break;
            }
        }
    }

    /**
     * Returns a listener that notes an Ask Gemini click before calling {@code listener}, which can
     * dismiss the menu.
     */
    SelectionDropdownMenuDelegate.ItemClickListener wrapDropdownClickListener(
            SelectionDropdownMenuDelegate.ItemClickListener listener) {
        return model -> {
            if (isAskGemini(model)) mAskGeminiClicked = true;
            listener.onItemClick(model);
        };
    }

    /** Called when the dropdown menu is dismissed. */
    void onDropdownMenuDismissed() {
        endSession();
    }

    private void endSession() {
        if (mAskGeminiShown) {
            GlicMetrics.recordWebContentsContextMenuResult(
                    GlicInvocationSource.WEB_CONTENTS_CONTEXT_MENU_TEXT_SELECTION,
                    mAskGeminiClicked);
        }
        mAskGeminiShown = false;
        mAskGeminiClicked = false;
    }

    private static boolean isAskGemini(PropertyModel model) {
        return model.containsKey(ListMenuItemProperties.MENU_ITEM_ID)
                && model.get(ListMenuItemProperties.MENU_ITEM_ID) == R.id.contextmenu_ask_gemini;
    }
}
