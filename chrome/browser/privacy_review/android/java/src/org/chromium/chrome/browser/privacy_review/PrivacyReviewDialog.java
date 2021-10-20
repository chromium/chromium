// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.privacy_review;

import android.app.Dialog;
import android.content.Context;
import android.view.MenuItem;
import android.view.View;
import android.widget.FrameLayout;

import androidx.appcompat.widget.Toolbar;
import androidx.viewpager2.widget.ViewPager2;

import org.chromium.ui.widget.ButtonCompat;

/**
 * UI for the Privacy Review dialog in Privacy and security settings.
 */
public class PrivacyReviewDialog extends Dialog {
    private View mDialogView;
    private ViewPager2 mViewPager;
    private PrivacyReviewPagerAdapter mPagerAdapter;

    public PrivacyReviewDialog(Context context) {
        super(context, R.style.ThemeOverlay_BrowserUI_Fullscreen);
        mDialogView = getLayoutInflater().inflate(R.layout.privacy_review_dialog, null);

        Toolbar toolbar = (Toolbar) mDialogView.findViewById(R.id.toolbar);
        toolbar.setTitle(R.string.prefs_privacy_review_title);
        toolbar.inflateMenu(R.menu.privacy_review_toolbar_menu);
        toolbar.setOnMenuItemClickListener(this::onMenuItemClick);

        setContentView(mDialogView);
        displayWelcomePage();
    }

    private boolean onMenuItemClick(MenuItem menuItem) {
        if (menuItem.getItemId() == R.id.close_menu_id) {
            dismiss();
            return true;
        }
        return false;
    }

    private void displayWelcomePage() {
        FrameLayout content = mDialogView.findViewById(R.id.dialog_content);
        content.removeAllViews();
        getLayoutInflater().inflate(R.layout.privacy_review_welcome, content);

        ButtonCompat welcomeButton = (ButtonCompat) mDialogView.findViewById(R.id.start_button);
        welcomeButton.setOnClickListener((View v) -> displayMainFlow());
    }

    private void displayMainFlow() {
        FrameLayout content = mDialogView.findViewById(R.id.dialog_content);
        content.removeAllViews();
        getLayoutInflater().inflate(R.layout.privacy_review_steps, content);

        mViewPager = (ViewPager2) mDialogView.findViewById(R.id.review_viewpager);
        mPagerAdapter = new PrivacyReviewPagerAdapter();
        mViewPager.setAdapter(mPagerAdapter);

        ButtonCompat nextButton = (ButtonCompat) mDialogView.findViewById(R.id.next_button);
        nextButton.setOnClickListener((View v) -> nextStep());

        ButtonCompat backButton = (ButtonCompat) mDialogView.findViewById(R.id.back_button);
        backButton.setOnClickListener((View v) -> previousStep());
    }

    private void nextStep() {
        int curIdx = mViewPager.getCurrentItem();
        if (curIdx + 1 < mPagerAdapter.getItemCount()) {
            mViewPager.setCurrentItem(curIdx + 1);
        }
    }

    private void previousStep() {
        int curIdx = mViewPager.getCurrentItem();
        if (curIdx > 0) {
            mViewPager.setCurrentItem(curIdx - 1);
        }
    }
}
