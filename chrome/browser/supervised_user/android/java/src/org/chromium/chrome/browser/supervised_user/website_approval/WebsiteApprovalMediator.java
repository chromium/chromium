// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.supervised_user.website_approval;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.content.Context;
import android.view.View;
import android.view.ViewGroup;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.supervised_user.R;
import org.chromium.components.browser_ui.adaptive_ui.AdaptiveUiContainer;
import org.chromium.components.browser_ui.adaptive_ui.AdaptiveUiController;
import org.chromium.components.browser_ui.adaptive_ui.AdaptiveUiEntry;
import org.chromium.components.browser_ui.adaptive_ui.AdaptiveUiProperties;
import org.chromium.components.browser_ui.adaptive_ui.AdaptiveUiProperties.CloseButtonVisibility;
import org.chromium.components.browser_ui.adaptive_ui.AdaptiveUiProperties.Delegate;
import org.chromium.components.browser_ui.adaptive_ui.AdaptiveUiProperties.SecondaryButtonBehavior;
import org.chromium.components.browser_ui.adaptive_ui.AdaptiveUiScope;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.UserCriticalFeature;
import org.chromium.components.signin.base.AccountInfo;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * Contains the logic for the WebsiteApproval component. It sets the state of the model and reacts
 * to events like clicks.
 */
@NullMarked
class WebsiteApprovalMediator {
    private final WebsiteApprovalCoordinator.CompletionCallback mCompletionCallback;
    private final @Nullable BottomSheetController mBottomSheetController;
    private final @Nullable AdaptiveUiController mAdaptiveUiController;
    private final WebsiteApprovalSheetContent mSheetContent;
    private final PropertyModel mModel;
    private final Profile mProfile;
    private boolean mCompleted;

    WebsiteApprovalMediator(
            WebsiteApprovalCoordinator.CompletionCallback completionCallback,
            @Nullable BottomSheetController bottomSheetController,
            @Nullable AdaptiveUiController adaptiveUiController,
            WebsiteApprovalSheetContent sheetContent,
            PropertyModel model,
            Profile profile) {
        mCompletionCallback = completionCallback;
        mBottomSheetController = bottomSheetController;
        mAdaptiveUiController = adaptiveUiController;
        mSheetContent = sheetContent;
        mModel = model;
        mProfile = profile;
    }

    void show() {
        // Set the child name.  We use the given name if there is one for this account, otherwise we
        // use the full account email address.
        IdentityManager identityManager =
                IdentityServicesProvider.get().getIdentityManager(mProfile);
        assumeNonNull(identityManager);
        @Nullable AccountInfo accountInfo = identityManager.getPrimaryAccountInfo();
        if (accountInfo == null) {
            // This is an unexpected window condition: there is no signed in account.
            // TODO(crbug.com/40843544): dismiss the bottom sheet.
            return;
        }

        mModel.set(
                WebsiteApprovalProperties.CHILD_NAME,
                accountInfo.getGivenName().isEmpty()
                        ? accountInfo.getEmail()
                        : accountInfo.getGivenName());

        if (mAdaptiveUiController != null) {
            showAdaptive(mAdaptiveUiController);
            return;
        }

        BottomSheetController bottomSheetController = assumeNonNull(mBottomSheetController);
        mModel.set(
                WebsiteApprovalProperties.ON_CLICK_APPROVE,
                v -> {
                    bottomSheetController.hideContent(
                            mSheetContent,
                            true,
                            BottomSheetController.StateChangeReason.INTERACTION_COMPLETE);
                    mCompletionCallback.onWebsiteApproved();
                });
        mModel.set(
                WebsiteApprovalProperties.ON_CLICK_DENY,
                v -> {
                    bottomSheetController.hideContent(
                            mSheetContent,
                            true,
                            BottomSheetController.StateChangeReason.INTERACTION_COMPLETE);
                    mCompletionCallback.onWebsiteDenied();
                });

        // Now show the actual content.
        bottomSheetController.requestShowContent(mSheetContent, true);
    }

    private void showAdaptive(AdaptiveUiController adaptiveUiController) {
        // TODO(crbug.com/581058945): Move the view inflation and setters (setTitle, setDomainText,
        // setFullUrlText, setFaviconBitmap) out of WebsiteApprovalSheetContent into a plain view
        // class so flag cleanup only deletes the legacy wrapper, the in-layout buttons, and the
        // button-hiding and padding code below.
        View approveButton = mSheetContent.getApproveButton();
        View denyButton = mSheetContent.getDenyButton();
        // Adaptive UI draws the buttons, so the layout keeps the gap that the hidden Approve
        // button's top margin used to add above them.
        int gapAboveButtons =
                ((ViewGroup.MarginLayoutParams) approveButton.getLayoutParams()).topMargin;
        approveButton.setVisibility(View.GONE);
        denyButton.setVisibility(View.GONE);

        View contentView = mSheetContent.getContentView();
        contentView.setPaddingRelative(
                contentView.getPaddingStart(),
                contentView.getPaddingTop(),
                contentView.getPaddingEnd(),
                gapAboveButtons);

        Context context = contentView.getContext();
        // TODO(crbug.com/581058945): Once the Adaptive UI sheet centers its header, pass the G
        // icon as HEADER_ICON and the title as TITLE, and hide them in the layout like the buttons.
        PropertyModel adaptiveModel =
                new PropertyModel.Builder(AdaptiveUiProperties.ALL_KEYS)
                        .with(AdaptiveUiProperties.CUSTOM_VIEW, contentView)
                        .with(
                                AdaptiveUiProperties.DIALOG_CONTENT_DESCRIPTION,
                                context.getString(
                                        R.string.parent_website_approval_content_description))
                        .with(
                                AdaptiveUiProperties.SHEET_OPENED_ACCESSIBILITY_STRING_ID,
                                R.string.parent_website_approval_full_height)
                        .with(
                                AdaptiveUiProperties.SHEET_CLOSED_ACCESSIBILITY_STRING_ID,
                                R.string.parent_website_approval_closed)
                        .with(
                                AdaptiveUiProperties.SHEET_USER_CRITICAL,
                                UserCriticalFeature.PARENTAL_APPROVAL)
                        .with(AdaptiveUiProperties.GESTURE_DISMISSAL_DISABLED, true)
                        .with(
                                AdaptiveUiProperties.CLOSE_BUTTON_VISIBILITY,
                                CloseButtonVisibility.HIDDEN)
                        .with(
                                AdaptiveUiProperties.PRIMARY_BUTTON_TEXT,
                                context.getString(R.string.parent_website_approval_approve_button))
                        .with(
                                AdaptiveUiProperties.SECONDARY_BUTTON_TEXT,
                                context.getString(R.string.parent_website_approval_deny_button))
                        .with(
                                AdaptiveUiProperties.SECONDARY_BUTTON_BEHAVIOR,
                                SecondaryButtonBehavior.ACTION_THEN_DISMISS)
                        .with(
                                AdaptiveUiProperties.DELEGATE,
                                new Delegate() {
                                    @Override
                                    public void onPrimaryButtonClicked() {
                                        onApproveClicked();
                                    }

                                    @Override
                                    public void onSecondaryButtonClicked() {
                                        onDenyClicked();
                                    }
                                })
                        .build();

        AdaptiveUiEntry entry =
                new AdaptiveUiEntry.Builder(adaptiveModel)
                        .setScope(AdaptiveUiScope.TRANSIENT)
                        .addAllowedContainer(AdaptiveUiContainer.BOTTOM_SHEET)
                        .addAllowedContainer(AdaptiveUiContainer.MODAL_DIALOG)
                        .build();
        adaptiveUiController.show(entry);
    }

    private void onApproveClicked() {
        if (mCompleted) return;
        mCompleted = true;
        mCompletionCallback.onWebsiteApproved();
    }

    private void onDenyClicked() {
        if (mCompleted) return;
        mCompleted = true;
        mCompletionCallback.onWebsiteDenied();
    }
}
