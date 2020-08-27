// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.signin.account_picker;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.ImageView;
import android.widget.TextView;

import androidx.annotation.Nullable;
import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.RecyclerView;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.signin.DisplayableProfileData;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;
import org.chromium.ui.widget.ButtonCompat;
import org.chromium.ui.widget.TextViewWithLeading;

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

    private final Context mContext;
    private final BackPressListener mBackPressListener;
    private final View mContentView;
    private final TextView mAccountPickerTitle;
    private final TextViewWithLeading mAccountPickerSubtitle;
    private final RecyclerView mAccountListView;
    private final View mSelectedAccountView;
    private final View mIncognitoInterstitialView;
    private final ButtonCompat mContinueAsButton;

    AccountPickerBottomSheetView(Context context, BackPressListener backPressListener) {
        mContext = context;
        mBackPressListener = backPressListener;
        mContentView = LayoutInflater.from(mContext).inflate(
                R.layout.account_picker_bottom_sheet_view, null);
        mAccountPickerTitle = mContentView.findViewById(R.id.account_picker_bottom_sheet_title);
        mAccountPickerSubtitle =
                mContentView.findViewById(R.id.account_picker_bottom_sheet_subtitle);
        mAccountListView = mContentView.findViewById(R.id.account_picker_account_list);
        mIncognitoInterstitialView =
                mContentView.findViewById(R.id.incognito_interstitial_bottom_sheet_view);
        mAccountListView.setLayoutManager(new LinearLayoutManager(
                mAccountListView.getContext(), LinearLayoutManager.VERTICAL, false));
        mSelectedAccountView = mContentView.findViewById(R.id.account_picker_selected_account);
        mContinueAsButton = mContentView.findViewById(R.id.account_picker_continue_as_button);
    }

    /**
     * The account list view is visible when the account list is expanded.
     */
    RecyclerView getAccountListView() {
        return mAccountListView;
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
     * Expands the account list.
     */
    void expandAccountList() {
        mSelectedAccountView.setVisibility(View.GONE);
        mContinueAsButton.setVisibility(View.GONE);
        mAccountListView.setVisibility(View.VISIBLE);
    }

    /**
     * Collapses the account list to the selected account.
     */
    void collapseAccountList() {
        mAccountListView.setVisibility(View.GONE);
        mSelectedAccountView.setVisibility(View.VISIBLE);
        mContinueAsButton.setVisibility(View.VISIBLE);
    }

    /**
     * Collapses the account list to the no account view.
     */
    void collapseToNoAccountView() {
        mAccountListView.setVisibility(View.GONE);
        mSelectedAccountView.setVisibility(View.GONE);
        mContinueAsButton.setVisibility(View.VISIBLE);
        mContinueAsButton.setText(R.string.signin_add_account_to_device);
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

        String continueAsButtonText = mContext.getString(R.string.signin_promo_continue_as,
                accountProfileData.getGivenNameOrFullNameOrEmail());
        mContinueAsButton.setText(continueAsButtonText);
    }

    /**
     * Sets up the sign-in in progress view.
     */
    void setUpSignInInProgressView() {
        mAccountPickerTitle.setText(R.string.signin_account_picker_bottom_sheet_signin_title);
        mAccountPickerSubtitle.setVisibility(View.INVISIBLE);
        // Set the account picker subtitle text in case there's an error.
        mAccountPickerSubtitle.setText(R.string.signin_account_picker_general_error_subtitle);
        mContentView.findViewById(R.id.account_picker_signin_spinner_view)
                .setVisibility(View.VISIBLE);
        mContinueAsButton.setVisibility(View.INVISIBLE);

        mContentView.findViewById(R.id.account_picker_horizontal_divider).setVisibility(View.GONE);
        mSelectedAccountView.setVisibility(View.GONE);
    }

    void setUpIncognitoInterstitialView() {
        // TODO(crbug.com/1103262): Setup the incognito interstitial strings.
        ImageView logo = mContentView.findViewById(R.id.account_picker_bottom_sheet_logo);
        logo.setImageResource(R.drawable.location_bar_incognito_badge);

        mAccountPickerTitle.setVisibility(View.GONE);
        mAccountPickerSubtitle.setVisibility(View.GONE);
        mContentView.findViewById(R.id.account_picker_horizontal_divider).setVisibility(View.GONE);
        mAccountListView.setVisibility(View.GONE);
        mIncognitoInterstitialView.setVisibility(View.VISIBLE);
    }

    /**
     * Sets up the view for sign-in general error.
     */
    void setUpSignInGeneralErrorView() {
        mAccountPickerTitle.setText(R.string.signin_account_picker_bottom_sheet_error_title);
        mAccountPickerSubtitle.setText(R.string.signin_account_picker_general_error_subtitle);
        mAccountPickerSubtitle.setVisibility(View.VISIBLE);
        mContentView.findViewById(R.id.account_picker_signin_spinner_view)
                .setVisibility(View.INVISIBLE);
        mContinueAsButton.setText(R.string.signin_account_picker_general_error_button);
        mContinueAsButton.setVisibility(View.VISIBLE);

        mContentView.findViewById(R.id.account_picker_horizontal_divider).setVisibility(View.GONE);
        mSelectedAccountView.setVisibility(View.GONE);
    }

    /**
     * Sets up the view for sign-in auth error.
     */
    void setUpSignInAuthErrorView() {
        mAccountPickerTitle.setText(R.string.signin_account_picker_bottom_sheet_error_title);
        mAccountPickerSubtitle.setText(R.string.signin_account_picker_auth_error_subtitle);
        mAccountPickerSubtitle.setVisibility(View.VISIBLE);
        mContentView.findViewById(R.id.account_picker_signin_spinner_view)
                .setVisibility(View.INVISIBLE);
        mContinueAsButton.setText(R.string.auth_error_card_button);
        mContinueAsButton.setVisibility(View.VISIBLE);

        mContentView.findViewById(R.id.account_picker_horizontal_divider).setVisibility(View.GONE);
        mSelectedAccountView.setVisibility(View.GONE);
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
        // TODO(https://crbug.com/1081253): The description will
        // be adapter once the UI mock will be finalized
        return R.string.signin_account_picker_dialog_title;
    }

    @Override
    public int getSheetHalfHeightAccessibilityStringId() {
        // TODO(https://crbug.com/1081253): The description will
        // be adapter once the UI mock will be finalized
        return R.string.signin_account_picker_dialog_title;
    }

    @Override
    public int getSheetFullHeightAccessibilityStringId() {
        // TODO(https://crbug.com/1081253): The description will
        // be adapter once the UI mock will be finalized
        return R.string.signin_account_picker_dialog_title;
    }

    @Override
    public int getSheetClosedAccessibilityStringId() {
        // TODO(https://crbug.com/1081253): The description will
        // be adapter once the UI mock will be finalized
        return R.string.signin_account_picker_dialog_title;
    }
}
