// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.privacy_review;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.MenuItem;
import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;

import androidx.appcompat.widget.Toolbar;
import androidx.viewpager2.widget.ViewPager2;

import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.ui.widget.ButtonCompat;

/**
 * UI for the Privacy Review dialog in Privacy and security settings.
 */
public class PrivacyReviewDialog {
    private LayoutInflater mLayoutInflater;
    private ViewGroup mContainer;
    private View mDialogView;
    private ViewPager2 mViewPager;
    private PrivacyReviewPagerAdapter mPagerAdapter;
    private ButtonCompat mNextButton;
    private ButtonCompat mBackButton;
    private ButtonCompat mFinishButton;
    private BottomSheetController mBottomSheetController;

    public PrivacyReviewDialog(
            Context context, ViewGroup container, BottomSheetController controller) {
        mContainer = container;
        mBottomSheetController = controller;
        mLayoutInflater = LayoutInflater.from(context);
        mDialogView = mLayoutInflater.inflate(R.layout.privacy_review_dialog, null);

        Toolbar toolbar = (Toolbar) mDialogView.findViewById(R.id.toolbar);
        toolbar.setTitle(R.string.prefs_privacy_review_title);
        toolbar.inflateMenu(R.menu.privacy_review_toolbar_menu);
        toolbar.setOnMenuItemClickListener(this::onMenuItemClick);

        displayWelcomePage();
    }

    /** Displays the dialog in a container given at construction time. */
    public void show() {
        mContainer.addView(mDialogView);
        mContainer.setVisibility(View.VISIBLE);
    }

    /** Hides the dialog. */
    public void dismiss() {
        mContainer.removeView(mDialogView);
        mContainer.setVisibility(View.GONE);
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
        mLayoutInflater.inflate(R.layout.privacy_review_welcome, content);

        ButtonCompat welcomeButton = (ButtonCompat) mDialogView.findViewById(R.id.start_button);
        welcomeButton.setOnClickListener((View v) -> displayMainFlow());
    }

    private void displayMainFlow() {
        FrameLayout content = mDialogView.findViewById(R.id.dialog_content);
        content.removeAllViews();
        mLayoutInflater.inflate(R.layout.privacy_review_steps, content);

        mViewPager = (ViewPager2) mDialogView.findViewById(R.id.review_viewpager);
        mPagerAdapter = new PrivacyReviewPagerAdapter(mBottomSheetController);
        mViewPager.setAdapter(mPagerAdapter);

        mNextButton = (ButtonCompat) mDialogView.findViewById(R.id.next_button);
        mNextButton.setOnClickListener((View v) -> nextStep());

        mBackButton = (ButtonCompat) mDialogView.findViewById(R.id.back_button);
        mBackButton.setOnClickListener((View v) -> previousStep());

        mFinishButton = (ButtonCompat) mDialogView.findViewById(R.id.finish_button);
        mFinishButton.setOnClickListener((View v) -> displayDonePage());
    }

    private void displayDonePage() {
        FrameLayout content = mDialogView.findViewById(R.id.dialog_content);
        content.removeAllViews();
        mLayoutInflater.inflate(R.layout.privacy_review_done, content);

        ButtonCompat doneButton = (ButtonCompat) mDialogView.findViewById(R.id.done_button);
        doneButton.setOnClickListener((View v) -> dismiss());
    }

    private void nextStep() {
        int nextIdx = mViewPager.getCurrentItem() + 1;
        if (nextIdx < mPagerAdapter.getItemCount()) {
            mViewPager.setCurrentItem(nextIdx);
        }
        mBackButton.setVisibility(View.VISIBLE);
        if (nextIdx + 1 == mPagerAdapter.getItemCount()) {
            mNextButton.setVisibility(View.GONE);
            mFinishButton.setVisibility(View.VISIBLE);
        }
    }

    private void previousStep() {
        mFinishButton.setVisibility(View.GONE);
        int prevIdx = mViewPager.getCurrentItem() - 1;
        if (prevIdx >= 0) {
            mViewPager.setCurrentItem(prevIdx);
        }
        mNextButton.setVisibility(View.VISIBLE);
        if (prevIdx == 0) {
            mBackButton.setVisibility(View.INVISIBLE);
        }
    }
}
