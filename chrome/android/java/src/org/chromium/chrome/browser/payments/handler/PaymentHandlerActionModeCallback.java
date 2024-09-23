// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments.handler;

import android.content.Intent;
import android.graphics.Rect;
import android.view.ActionMode;
import android.view.Menu;
import android.view.MenuItem;
import android.view.View;

import androidx.annotation.Nullable;

import org.chromium.content_public.browser.ActionModeCallback;
import org.chromium.content_public.browser.ActionModeCallbackHelper;
import org.chromium.content_public.browser.SelectionPopupController;
import org.chromium.content_public.browser.WebContents;

/** A class that handles selection action mode for Payment Handler. */
public class PaymentHandlerActionModeCallback extends ActionModeCallback {
    private final ActionModeCallbackHelper mHelper;

    /**
     * Create the PaymentHandler action mode callback.
     * @param webContents The WebContents from which the action mode is triggered.
     */
    /* package */ PaymentHandlerActionModeCallback(WebContents webContents) {
        mHelper =
                SelectionPopupController.fromWebContents(webContents).getActionModeCallbackHelper();
        mHelper.setAllowedMenuItems(0); // No item is allowed by default for WebView.
    }

    @Override
    public boolean onCreateActionMode(ActionMode mode, Menu menu) {
        mHelper.onCreateActionMode(mode, menu);
        return true;
    }

    @Override
    public boolean onPrepareActionMode(ActionMode mode, Menu menu) {
        return mHelper.onPrepareActionMode(mode, menu);
    }

    @Override
    public boolean onActionItemClicked(ActionMode mode, MenuItem item) {
        if (!mHelper.isActionModeValid()) return true;

        return mHelper.onActionItemClicked(mode, item);
    }

    @Override
    public boolean onDropdownItemClicked(
            int groupId,
            int id,
            @Nullable Intent intent,
            @Nullable View.OnClickListener clickListener) {
        return mHelper.onDropdownItemClicked(groupId, id, intent, clickListener);
    }

    @Override
    public void onDestroyActionMode(ActionMode mode) {
        mHelper.onDestroyActionMode();
    }

    @Override
    public void onGetContentRect(ActionMode mode, View view, Rect outRect) {
        mHelper.onGetContentRect(mode, view, outRect);
    }
}
