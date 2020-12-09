// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.signin.account_picker;

import android.app.Activity;
import android.view.LayoutInflater;
import android.view.View;
import android.view.accessibility.AccessibilityEvent;
import android.widget.ImageView;
import android.widget.ProgressBar;
import android.widget.TextView;

import androidx.annotation.Nullable;
import androidx.annotation.StringRes;
import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.RecyclerView;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.signin.DisplayableProfileData;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;
import org.chromium.ui.widget.ButtonCompat;

/**
 * This class is the AccountPickerBottomsheet view for the web sign-in flow.
 *
 * The bottom sheet shows a single account with a |Continue as ...| button by default, clicking
 * on the account will expand the bottom sheet to an account list together with other sign-in
 * options like "Add account" and "Go incognito mode".
 */
class AccountPickerBottomSheetView implements BottomSheetContent {
    /**
     * Listener for the back-press button.
     */
    interface BackPressListener {
        /**
         * Notifies when user clicks the back-press button.
         * @return true if the listener handles the back press, false if not.
         */
        boolean onBackPressed();
    }

    private final Activity mActivity;
    private final BackPressListener mBackPressListener;
    private final View mContentView;
    private final ImageView mLogoImage;
    private final TextView mAccountPickerTitle;
    private final TextView mAccountPickerSubtitle;
    private final View mHorizontalDivider;
    private final RecyclerView mAccountListView;
    private final View mSelectedAccountView;
    private final View mIncognitoInterstitialView;
    private final ProgressBar mSpinnerView;
    private final ButtonCompat mContinueAsButton;
    private final ButtonCompat mDismissButton;

    private @StringRes int mTitleId;
    private @StringRes int mContentDescriptionId;

    /**
     * @param activity The activity that hosts this view. Used for inflating views.
     * @param backPressListener The listener to be notified when the user taps the back button.
     */
    AccountPickerBottomSheetView(Activity activity, BackPressListener backPressListener) {
        mActivity = activity;
        mBackPressListener = backPressListener;

        mContentView = LayoutInflater.from(mActivity).inflate(
                R.layout.account_picker_bottom_sheet_view, null);
        mLogoImage = mContentView.findViewById(R.id.account_picker_bottom_sheet_logo);
        mAccountPickerTitle = mContentView.findViewById(R.id.account_picker_bottom_sheet_title);
        mAccountPickerSubtitle =
                mContentView.findViewById(R.id.account_picker_bottom_sheet_subtitle);
        mHorizontalDivider = mContentView.findViewById(R.id.account_picker_horizontal_divider);
        mAccountListView = mContentView.findViewById(R.id.account_picker_account_list);
        mIncognitoInterstitialView =
                mContentView.findViewById(R.id.incognito_interstitial_bottom_sheet_view);
        mAccountListView.setLayoutManager(new LinearLayoutManager(
                mAccountListView.getContext(), LinearLayoutManager.VERTICAL, false));
        mSelectedAccountView = mContentView.findViewById(R.id.account_picker_selected_account);
        mSpinnerView = mContentView.findViewById(R.id.account_picker_signin_spinner_view);
        mContinueAsButton = mContentView.findViewById(R.id.account_picker_continue_as_button);
        mDismissButton = mContentView.findViewById(R.id.account_picker_dismiss_button);
    }

    void setTitleAndContentDescriptionStrings(
            @StringRes int titleId, @StringRes @Nullable Integer subtitleId) {
        mTitleId = titleId;
        mContentDescriptionId = subtitleId != null ? subtitleId : titleId;
    }

    /**
     * The account list view is visible when the account list is expanded.
     */
    RecyclerView getAccountListView() {
        return mAccountListView;
    }

    /**
     * The incognito interstitial view when the user clicks on incognito mode option.
     */
    View getIncognitoInterstitialView() {
        return mIncognitoInterstitialView;
    }

    /**
     * The selected account is visible when the account list is collapsed.
     */
    View getSelectedAccountView() {
        return mSelectedAccountView;
    }

    /**
     * The |Continue As| button on the bottom sheet.
     */
    ButtonCompat getContinueAsButton() {
        return mContinueAsButton;
    }

    /**
     * The button to dismiss the bottom sheet.
     */
    ButtonCompat getDismissButton() {
        return mDismissButton;
    }

    /**
     * Updates the views related to the selected account.
     *
     * This method only updates the UI elements like text related to the selected account, it
     * does not change the visibility.
     */
    void updateSelectedAccount(DisplayableProfileData accountProfileData) {
        ExistingAccountRowViewBinder.bindAccountView(accountProfileData, mSelectedAccountView);

        ImageView rowEndImage = mSelectedAccountView.findViewById(R.id.account_selection_mark);
        rowEndImage.setImageResource(R.drawable.ic_expand_more_in_circle_24dp);

        String continueAsButtonText = mActivity.getString(R.string.signin_promo_continue_as,
                accountProfileData.getGivenNameOrFullNameOrEmail());
        mContinueAsButton.setText(continueAsButtonText);
        mAccountPickerTitle.setFocusable(true);
    }

    /**
     * Set A11y focus on title
     */
    void setAccessibilityFocusOnTitle() {
        mAccountPickerTitle.sendAccessibilityEvent(AccessibilityEvent.TYPE_VIEW_FOCUSED);
    }

    /**
     * Expands the account list.
     */
    void expandAccountList() {
        mLogoImage.setImageResource(R.drawable.chrome_sync_logo);
        mAccountPickerTitle.setText(R.string.signin_account_picker_dialog_title);
        mAccountPickerSubtitle.setText(R.string.signin_account_picker_bottom_sheet_subtitle);
        mAccountPickerSubtitle.setVisibility(View.VISIBLE);
        mHorizontalDivider.setVisibility(View.VISIBLE);
        mAccountListView.setVisibility(View.VISIBLE);

        mSelectedAccountView.setVisibility(View.GONE);
        mContinueAsButton.setVisibility(View.GONE);
        mDismissButton.setVisibility(View.GONE);
        mIncognitoInterstitialView.setVisibility(View.GONE);
    }

    /**
     * Collapses the account list to the selected account.
     */
    void collapseAccountList() {
        mLogoImage.setImageResource(R.drawable.chrome_sync_logo);
        mAccountPickerTitle.setText(R.string.signin_account_picker_dialog_title);
        mAccountPickerSubtitle.setText(R.string.signin_account_picker_bottom_sheet_subtitle);
        mHorizontalDivider.setVisibility(View.VISIBLE);
        mSelectedAccountView.setVisibility(View.VISIBLE);
        mContinueAsButton.setVisibility(View.VISIBLE);
        mDismissButton.setVisibility(View.VISIBLE);

        mAccountListView.setVisibility(View.GONE);
        mSpinnerView.setVisibility(View.GONE);
    }

    /**
     * Collapses the account list to the no account view.
     */
    void collapseToNoAccountView() {
        mContinueAsButton.setText(R.string.signin_add_account_to_device);
        mContinueAsButton.setVisibility(View.VISIBLE);
        mDismissButton.setVisibility(View.VISIBLE);

        mAccountListView.setVisibility(View.GONE);
        mSelectedAccountView.setVisibility(View.GONE);
    }

    /**
     * Sets up the sign-in in progress view.
     */
    void setUpSignInInProgressView() {
        mLogoImage.setImageResource(R.drawable.chrome_sync_logo);
        mAccountPickerTitle.setText(R.string.signin_account_picker_bottom_sheet_signin_title);
        mSpinnerView.setVisibility(View.VISIBLE);

        mAccountPickerSubtitle.setVisibility(View.GONE);
        mHorizontalDivider.setVisibility(View.GONE);
        mSelectedAccountView.setVisibility(View.GONE);
        mContinueAsButton.setVisibility(View.GONE);
        mDismissButton.setVisibility(View.GONE);
    }

    void setUpIncognitoInterstitialView() {
        mLogoImage.setImageResource(R.drawable.ic_incognito_filled_24dp);
        mAccountPickerTitle.setText(R.string.incognito_interstitial_title);
        mIncognitoInterstitialView.setVisibility(View.VISIBLE);

        mAccountPickerSubtitle.setVisibility(View.GONE);
        mHorizontalDivider.setVisibility(View.GONE);
        mAccountListView.setVisibility(View.GONE);
    }

    /**
     * Sets up the view for sign-in general error.
     */
    void setUpSignInGeneralErrorView() {
        mLogoImage.setImageResource(R.drawable.ic_warning_red_24dp);
        mAccountPickerTitle.setText(R.string.signin_account_picker_bottom_sheet_error_title);
        mAccountPickerSubtitle.setText(R.string.signin_account_picker_general_error_subtitle);
        mAccountPickerSubtitle.setVisibility(View.VISIBLE);
        mContinueAsButton.setText(R.string.signin_account_picker_general_error_button);
        mContinueAsButton.setVisibility(View.VISIBLE);

        mHorizontalDivider.setVisibility(View.GONE);
        mSelectedAccountView.setVisibility(View.GONE);
        mSpinnerView.setVisibility(View.GONE);
        mDismissButton.setVisibility(View.GONE);
    }

    /**
     * Sets up the view for sign-in auth error.
     */
    void setUpSignInAuthErrorView() {
        mLogoImage.setImageResource(R.drawable.ic_warning_red_24dp);
        mAccountPickerTitle.setText(R.string.signin_account_picker_bottom_sheet_error_title);
        mAccountPickerSubtitle.setText(R.string.signin_account_picker_auth_error_subtitle);
        mAccountPickerSubtitle.setVisibility(View.VISIBLE);
        mContinueAsButton.setText(R.string.auth_error_card_button);
        mContinueAsButton.setVisibility(View.VISIBLE);

        mHorizontalDivider.setVisibility(View.GONE);
        mSelectedAccountView.setVisibility(View.GONE);
        mSpinnerView.setVisibility(View.GONE);
        mDismissButton.setVisibility(View.GONE);
    }

    @Override
    public View getContentView() {
        return mContentView;
    }

    @Nullable
    @Override
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
    public float getFullHeightRatio() {
        return HeightMode.WRAP_CONTENT;
    }

    @Override
    public void destroy() {}

    @Override
    public int getPriority() {
        return ContentPriority.HIGH;
    }

    @Override
    public boolean swipeToDismissEnabled() {
        return true;
    }

    @Override
    public boolean handleBackPress() {
        return mBackPressListener.onBackPressed();
    }

    @Override
    public int getSheetContentDescriptionStringId() {
        return mContentDescriptionId;
    }

    @Override
    public int getSheetHalfHeightAccessibilityStringId() {
        return mTitleId;
    }

    @Override
    public int getSheetFullHeightAccessibilityStringId() {
        return mTitleId;
    }

    @Override
    public int getSheetClosedAccessibilityStringId() {
        // TODO(https://crbug.com/1112696): Use more specific string to when the account
        // picker is closed.
        return R.string.close;
    }
}
