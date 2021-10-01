// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.privacy_review;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.MenuItem;
import android.view.View;

import androidx.appcompat.app.AlertDialog;
import androidx.appcompat.widget.Toolbar;

/**
 * UI for the Privacy Review dialog in Privacy and security settings.
 */
public class PrivacyReviewDialog extends AlertDialog {
    public PrivacyReviewDialog(Context context) {
        super(context, R.style.ThemeOverlay_BrowserUI_Fullscreen);
        View view = LayoutInflater.from(context).inflate(R.layout.privacy_review_dialog, null);
        Toolbar toolbar = (Toolbar) view.findViewById(R.id.toolbar);
        toolbar.setTitle(R.string.prefs_privacy_review_title);
        toolbar.inflateMenu(R.menu.privacy_review_toolbar_menu);
        toolbar.setOnMenuItemClickListener(this::onMenuItemClick);

        setView(view);
    }

    private boolean onMenuItemClick(MenuItem menuItem) {
        if (menuItem.getItemId() == R.id.close_menu_id) {
            dismiss();
            return true;
        }
        return false;
    }
}
