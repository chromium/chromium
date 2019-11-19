// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share.qrcode;

import android.content.Context;
import android.support.design.widget.TabLayout;
import android.support.v4.view.ViewPager;
import android.support.v7.app.AlertDialog;
import android.view.LayoutInflater;
import android.view.View;

import org.chromium.chrome.R;
import org.chromium.ui.widget.ChromeImageButton;

import java.util.ArrayList;

/**
 * QrCodeDialog is the main view for QR code sharing and scanning.
 */
public class QrCodeDialog extends AlertDialog {
    private final TabLayout mTabLayout;
    private final ViewPager mViewPager;

    /**
     * The QrCodeDialog constructor.
     * @param context The context to use.
     * @param shareView The view for displaying in the share tab.
     * @param scanView The view for displaying in the scan tab.
     */
    public QrCodeDialog(Context context, View shareView, View scanView) {
        super(context, R.style.Theme_Chromium_Fullscreen);

        View dialogView = (View) LayoutInflater.from(context).inflate(
                org.chromium.chrome.browser.share.qrcode.R.layout.qrcode_dialog, null);
        ChromeImageButton closeButton =
                (ChromeImageButton) dialogView.findViewById(R.id.close_button);
        closeButton.setOnClickListener(v -> cancel());

        // Setup page adapter and tab layout.
        ArrayList<View> pages = new ArrayList<View>();
        pages.add(shareView);
        pages.add(scanView);
        QrCodePageAdapter pageAdapter = new QrCodePageAdapter(pages);

        mTabLayout =
                dialogView.findViewById(org.chromium.chrome.browser.share.qrcode.R.id.tab_layout);
        mViewPager = dialogView.findViewById(
                org.chromium.chrome.browser.share.qrcode.R.id.qrcode_view_pager);
        mViewPager.setAdapter(pageAdapter);
        mViewPager.addOnPageChangeListener(new TabLayout.TabLayoutOnPageChangeListener(mTabLayout));
        mTabLayout.addOnTabSelectedListener(
                new TabLayout.ViewPagerOnTabSelectedListener(mViewPager));
        setView(dialogView);
    }
}
