// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.supervised_user.website_approval;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.LinearLayout;
import android.widget.TextView;

import androidx.annotation.Nullable;
import androidx.appcompat.widget.DialogTitle;

import org.chromium.chrome.browser.supervised_user.R;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;
import org.chromium.ui.widget.ButtonCompat;

/**
 * Bottom sheet content for the screen which allows a parent to approve or deny a website.
 */
class WebsiteApprovalSheetContent implements BottomSheetContent {
    private static final String TAG = "WebsiteApprovalSheetContent";
    private final Context mContext;
    private final View mContentView;

    public WebsiteApprovalSheetContent(Context context) {
        mContext = context;
        mContentView = (LinearLayout) LayoutInflater.from(mContext).inflate(
                R.layout.website_approval_bottom_sheet, null);
    }

    @Override
    public View getContentView() {
        return mContentView;
    }

    @Override
    @Nullable
    public View getToolbarView() {
        return null;
    }

    @Override
    public int getVerticalScrollOffset() {
        return 0;
    }

    @Override
    public int getPeekHeight() {
        return HeightMode.DISABLED;
    }

    @Override
    public float getHalfHeightRatio() {
        return HeightMode.DISABLED;
    }

    @Override
    public float getFullHeightRatio() {
        return HeightMode.WRAP_CONTENT;
    }

    @Override
    public void destroy() {}

    @Override
    @ContentPriority
    public int getPriority() {
        return ContentPriority.HIGH;
    }

    @Override
    public boolean swipeToDismissEnabled() {
        // We don't want the sheet to be too easily dismissed, as getting here requires a full
        // manual password entry.
        return false;
    }

    @Override
    public int getSheetContentDescriptionStringId() {
        return R.string.parent_website_approval_content_description;
    }

    @Override
    public int getSheetHalfHeightAccessibilityStringId() {
        // Half-height is disabled so no need for an accessibility string.
        return 0;
    }

    @Override
    public int getSheetFullHeightAccessibilityStringId() {
        return R.string.parent_website_approval_full_height;
    }

    @Override
    public int getSheetClosedAccessibilityStringId() {
        return R.string.parent_website_approval_closed;
    }

    public ButtonCompat getApproveButton() {
        return mContentView.findViewById(R.id.approve_button);
    }

    public ButtonCompat getDenyButton() {
        return mContentView.findViewById(R.id.deny_button);
    }

    public void setTitle(String childName) {
        DialogTitle titleView = mContentView.findViewById(R.id.website_approval_sheet_title);
        titleView.setText(mContext.getString(R.string.parent_website_approval_title, childName));
    }

    public void setDomainText(String domain) {
        TextView domainTextView = mContentView.findViewById(R.id.all_pages_of);
        domainTextView.setText(
                mContext.getString(R.string.parent_website_approval_all_of_domain, domain));
    }

    public void setFullUrlText(String url) {
        TextView urlTextView = mContentView.findViewById(R.id.full_url);
        urlTextView.setText(url);
    }
}
