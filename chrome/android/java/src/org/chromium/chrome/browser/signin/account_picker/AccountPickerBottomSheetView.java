// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.signin.account_picker;

import android.app.Activity;
import android.view.LayoutInflater;
import android.view.View;
import android.view.View.OnClickListener;
import android.widget.ImageView;
import android.widget.ViewFlipper;

import androidx.annotation.IdRes;
import androidx.annotation.Nullable;
import androidx.annotation.StringRes;
import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.RecyclerView;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.signin.DisplayableProfileData;
import org.chromium.chrome.browser.signin.account_picker.AccountPickerBottomSheetProperties.ViewState;
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
    private final ViewFlipper mViewFlipper;
    private final RecyclerView mAccountListView;
    private final View mSelectedAccountView;
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

        mViewFlipper = mContentView.findViewById(R.id.account_picker_state_view_flipper);
        checkViewFlipperChildrenAndViewStateMatch(mViewFlipper);
        mAccountListView = mViewFlipper.getChildAt(ViewState.EXPANDED_ACCOUNT_LIST)
                                   .findViewById(R.id.account_picker_account_list);
        mAccountListView.setLayoutManager(new LinearLayoutManager(
                mAccountListView.getContext(), LinearLayoutManager.VERTICAL, false));
        mSelectedAccountView = mViewFlipper.getChildAt(ViewState.COLLAPSED_ACCOUNT_LIST)
                                       .findViewById(R.id.account_picker_selected_account);
        mDismissButton = mViewFlipper.getChildAt(ViewState.COLLAPSED_ACCOUNT_LIST)
                                 .findViewById(R.id.account_picker_dismiss_button);
        if (AccountPickerFeatureUtils.shouldShowNoThanksOnDismissButton()) {
            mDismissButton.setText(R.string.no_thanks);
        }

        // TODO(https://crbug.com/1146990): Use different continue buttons for different view
        // states.
        setUpContinueButton(mViewFlipper.getChildAt(ViewState.NO_ACCOUNTS),
                R.string.signin_add_account_to_device);
        setUpContinueButton(mViewFlipper.getChildAt(ViewState.SIGNIN_GENERAL_ERROR),
                R.string.signin_account_picker_general_error_button);
        setUpContinueButton(mViewFlipper.getChildAt(ViewState.SIGNIN_AUTH_ERROR),
                R.string.auth_error_card_button);
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
        return mViewFlipper.getChildAt(ViewState.INCOGNITO_INTERSTITIAL);
    }

    /**
     * The selected account is visible when the account list is collapsed.
     */
    View getSelectedAccountView() {
        return mSelectedAccountView;
    }

    /**
     * Sets the listener of the continue button.
     * TODO(https://crbug.com/1146990): Use different continue buttons for different view states.
     */
    void setOnClickListenerOfContinueButton(OnClickListener listener) {
        for (int i = 0; i < mViewFlipper.getChildCount(); ++i) {
            ButtonCompat continueButton =
                    mViewFlipper.getChildAt(i).findViewById(R.id.account_picker_continue_as_button);
            if (continueButton != null) {
                continueButton.setOnClickListener(listener);
            }
        }
    }

    /**
     * The button to dismiss the bottom sheet.
     */
    ButtonCompat getDismissButton() {
        return mDismissButton;
    }

    /**
     * Sets the displayed view according to the given {@link ViewState}.
     */
    void setDisplayedView(@ViewState int state) {
        mViewFlipper.setDisplayedChild(state);
    }

    /**
     * Updates the views related to the selected account.
     *
     * This method only updates the UI elements like text related to the selected account, it
     * does not change the visibility.
     */
    void updateSelectedAccount(DisplayableProfileData accountProfileData) {
        View view = mViewFlipper.getChildAt(ViewState.COLLAPSED_ACCOUNT_LIST);
        ExistingAccountRowViewBinder.bindAccountView(accountProfileData, mSelectedAccountView);

        ImageView rowEndImage = mSelectedAccountView.findViewById(R.id.account_selection_mark);
        rowEndImage.setImageResource(R.drawable.ic_expand_more_in_circle_24dp);

        ButtonCompat continueButton = view.findViewById(R.id.account_picker_continue_as_button);
        String continueAsButtonText = mActivity.getString(R.string.signin_promo_continue_as,
                accountProfileData.getGivenNameOrFullNameOrEmail());
        continueButton.setText(continueAsButtonText);
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

    private static void setUpContinueButton(View view, @StringRes int buttonId) {
        ButtonCompat continueButton = view.findViewById(R.id.account_picker_continue_as_button);
        continueButton.setText(buttonId);
    }

    private static void checkViewFlipperChildrenAndViewStateMatch(ViewFlipper viewFlipper) {
        checkViewFlipperChildIdAndViewStateMatch(
                viewFlipper, ViewState.NO_ACCOUNTS, R.id.account_picker_state_no_account);
        checkViewFlipperChildIdAndViewStateMatch(
                viewFlipper, ViewState.COLLAPSED_ACCOUNT_LIST, R.id.account_picker_state_collapsed);
        checkViewFlipperChildIdAndViewStateMatch(
                viewFlipper, ViewState.EXPANDED_ACCOUNT_LIST, R.id.account_picker_state_expanded);
        checkViewFlipperChildIdAndViewStateMatch(viewFlipper, ViewState.SIGNIN_IN_PROGRESS,
                R.id.account_picker_state_signin_in_progress);
        checkViewFlipperChildIdAndViewStateMatch(viewFlipper, ViewState.INCOGNITO_INTERSTITIAL,
                R.id.account_picker_state_incognito_interstitial);
        checkViewFlipperChildIdAndViewStateMatch(viewFlipper, ViewState.SIGNIN_GENERAL_ERROR,
                R.id.account_picker_state_general_error);
        checkViewFlipperChildIdAndViewStateMatch(
                viewFlipper, ViewState.SIGNIN_AUTH_ERROR, R.id.account_picker_state_auth_error);
    }

    private static void checkViewFlipperChildIdAndViewStateMatch(
            ViewFlipper viewFlipper, @ViewState int viewState, @IdRes int expectedChildId) {
        if (viewFlipper.getChildAt(viewState).getId() != expectedChildId) {
            throw new IllegalArgumentException("Match failed with ViewState:" + viewState);
        }
    }
}
