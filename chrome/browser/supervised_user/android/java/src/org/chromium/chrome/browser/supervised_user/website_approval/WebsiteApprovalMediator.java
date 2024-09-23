// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.supervised_user.website_approval;

import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.signin.base.AccountInfo;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.signin.identitymanager.ConsentLevel;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * Contains the logic for the WebsiteApproval component. It sets the state of the model and reacts
 * to events like clicks.
 */
class WebsiteApprovalMediator {
    private final WebsiteApprovalCoordinator.CompletionCallback mCompletionCallback;
    private final BottomSheetController mBottomSheetController;
    private final WebsiteApprovalSheetContent mSheetContent;
    private final PropertyModel mModel;
    private final Profile mProfile;

    WebsiteApprovalMediator(
            WebsiteApprovalCoordinator.CompletionCallback completionCallback,
            BottomSheetController bottomSheetController,
            WebsiteApprovalSheetContent sheetContent,
            PropertyModel model,
            Profile profile) {
        mCompletionCallback = completionCallback;
        mBottomSheetController = bottomSheetController;
        mSheetContent = sheetContent;
        mModel = model;
        mProfile = profile;
    }

    void show() {
        mModel.set(
                WebsiteApprovalProperties.ON_CLICK_APPROVE,
                v -> {
                    mBottomSheetController.hideContent(
                            mSheetContent,
                            true,
                            BottomSheetController.StateChangeReason.INTERACTION_COMPLETE);
                    mCompletionCallback.onWebsiteApproved();
                });
        mModel.set(
                WebsiteApprovalProperties.ON_CLICK_DENY,
                v -> {
                    mBottomSheetController.hideContent(
                            mSheetContent,
                            true,
                            BottomSheetController.StateChangeReason.INTERACTION_COMPLETE);
                    mCompletionCallback.onWebsiteDenied();
                });

        // Set the child name.  We use the given name if there is one for this account, otherwise we
        // use the full account email address.
        IdentityManager identityManager =
                IdentityServicesProvider.get().getIdentityManager(mProfile);
        String childEmail =
                CoreAccountInfo.getEmailFrom(
                        identityManager.getPrimaryAccountInfo(ConsentLevel.SIGNIN));
        if (childEmail == null) {
            // This is an unexpected window condition: there is no signed in account.
            // TODO(crbug.com/40843544): dismiss the bottom sheet.
            return;
        }
        AccountInfo childAccountInfo =
                identityManager.findExtendedAccountInfoByEmailAddress(childEmail);

        String childNameProperty = childEmail;
        if (childAccountInfo != null && !childAccountInfo.getGivenName().isEmpty()) {
            childNameProperty = childAccountInfo.getGivenName();
        }

        mModel.set(WebsiteApprovalProperties.CHILD_NAME, childNameProperty);

        // Now show the actual content.
        mBottomSheetController.requestShowContent(mSheetContent, true);
    }
}
