// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview;

import android.app.Activity;
import android.app.SearchManager;
import android.content.ActivityNotFoundException;
import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.graphics.Rect;
import android.text.TextUtils;
import android.view.ActionMode;
import android.view.Menu;
import android.view.MenuItem;
import android.view.View;

import org.chromium.android_webview.common.Lifetime;
import org.chromium.base.ContextUtils;
import org.chromium.base.PackageManagerUtils;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.content_public.browser.ActionModeCallback;
import org.chromium.content_public.browser.ActionModeCallbackHelper;
import org.chromium.content_public.browser.SelectionMenuItem;
import org.chromium.content_public.browser.SelectionPopupController;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.WindowAndroid.IntentCallback;

/** A class that handles selection action mode for Android WebView. */
@Lifetime.WebView
public class AwActionModeCallback extends ActionModeCallback implements IntentCallback {
    private final Context mContext;
    private final AwContents mAwContents;
    private final SelectionPopupController mSelectionPopupController;
    private final ActionModeCallbackHelper mHelper;
    private int mAllowedMenuItems;

    public AwActionModeCallback(Context context, AwContents awContents, WebContents webContents) {
        mContext = context;
        mAwContents = awContents;
        mSelectionPopupController = SelectionPopupController.fromWebContents(webContents);
        mHelper = mSelectionPopupController.getActionModeCallbackHelper();
        mHelper.setAllowedMenuItems(0); // No item is allowed by default for WebView.
    }

    @Override
    public boolean onCreateActionMode(ActionMode mode, Menu menu) {
        int allowedItems =
                (getAllowedMenu(ActionModeCallbackHelper.MENU_ITEM_SHARE)
                        | getAllowedMenu(ActionModeCallbackHelper.MENU_ITEM_WEB_SEARCH)
                        | getAllowedMenu(ActionModeCallbackHelper.MENU_ITEM_PROCESS_TEXT));
        if (allowedItems != mAllowedMenuItems) {
            mHelper.setAllowedMenuItems(allowedItems);
            mAllowedMenuItems = allowedItems;
        }
        mHelper.onCreateActionMode(mode, menu);
        return true;
    }

    private int getAllowedMenu(int menuItem) {
        boolean showItem = true;
        if (menuItem == ActionModeCallbackHelper.MENU_ITEM_WEB_SEARCH) {
            showItem = isWebSearchAvailable();
        }
        return showItem && mAwContents.isSelectActionModeAllowed(menuItem) ? menuItem : 0;
    }

    private boolean isWebSearchAvailable() {
        Intent intent = new Intent(Intent.ACTION_WEB_SEARCH);
        intent.putExtra(SearchManager.EXTRA_NEW_SEARCH, true);
        return !PackageManagerUtils.queryIntentActivities(intent, PackageManager.MATCH_DEFAULT_ONLY)
                .isEmpty();
    }

    @Override
    public boolean onPrepareActionMode(ActionMode mode, Menu menu) {
        return mHelper.onPrepareActionMode(mode, menu);
    }

    @Override
    public boolean onActionItemClicked(ActionMode mode, MenuItem item) {
        if (!mHelper.isActionModeValid()) return true;

        if (isProcessTextMenuItem(item.getGroupId())) {
            processText(item.getIntent());
            // The ActionMode is not dismissed to match the behavior with
            // TextView in Android M.
            return true;
        }

        return mHelper.onActionItemClicked(mode, item);
    }

    @Override
    public boolean onDropdownItemClicked(SelectionMenuItem item, boolean closeMenu) {
        if (isProcessTextMenuItem(item.groupId)) {
            assert item.intent != null
                    : "Text processing item must have an intent associated with it";
            processText(item.intent);
            return true;
        }
        return mHelper.onDropdownItemClicked(item, closeMenu);
    }

    private boolean isProcessTextMenuItem(final int groupId) {
        return groupId == R.id.select_action_menu_text_processing_items;
    }

    @Override
    public void onDestroyActionMode(ActionMode mode) {
        mHelper.onDestroyActionMode();
    }

    @Override
    public void onGetContentRect(ActionMode mode, View view, Rect outRect) {
        mHelper.onGetContentRect(mode, view, outRect);
    }

    private void processText(Intent intent) {
        RecordUserAction.record("MobileActionMode.ProcessTextIntent");
        String query =
                ActionModeCallbackHelper.sanitizeQuery(
                        mHelper.getSelectedText(),
                        ActionModeCallbackHelper.MAX_SEARCH_QUERY_LENGTH);
        if (TextUtils.isEmpty(query)) return;

        intent.putExtra(Intent.EXTRA_PROCESS_TEXT, query);

        if (ContextUtils.activityFromContext(mContext) == null) {
            try {
                mContext.startActivity(intent);
            } catch (ActivityNotFoundException ex) {
                // If no app handles it, do nothing.
            }
            return;
        }

        mAwContents.startActivityForResult(intent, /* callback= */ this);
    }

    // IntentCallback:
    @Override
    public void onIntentCompleted(int resultCode, Intent data) {
        if (resultCode == Activity.RESULT_OK && data != null) {
            CharSequence value = data.getCharSequenceExtra(Intent.EXTRA_PROCESS_TEXT);
            String result = (value == null) ? null : value.toString();
            mSelectionPopupController.handleTextReplacementAction(result);
        }
    }
}
