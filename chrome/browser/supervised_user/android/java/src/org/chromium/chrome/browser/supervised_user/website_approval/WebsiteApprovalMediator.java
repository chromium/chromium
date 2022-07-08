// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.supervised_user.website_approval;

import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
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
    private final PropertyModel mModel;

    WebsiteApprovalMediator(
            WebsiteApprovalCoordinator.CompletionCallback completionCallback, PropertyModel model) {
        mCompletionCallback = completionCallback;
        mModel = model;
    }

    void show() {
        mModel.set(WebsiteApprovalProperties.ON_CLICK_APPROVE,
                v -> mCompletionCallback.onWebsiteApproved());
        mModel.set(WebsiteApprovalProperties.ON_CLICK_DENY,
                v -> mCompletionCallback.onWebsiteDenied());

        // Set the child name.  We use the given name if there is one for this account, otherwise we
        // use the full account email address.
        IdentityManager identityManager = IdentityServicesProvider.get().getIdentityManager(
                Profile.getLastUsedRegularProfile());
        String childEmail = CoreAccountInfo.getEmailFrom(
                identityManager.getPrimaryAccountInfo(ConsentLevel.SIGNIN));
        if (childEmail == null) {
            // This is an unexpected window condition: there is no signed in account.
            // TODO(crbug.com/1330900): dismiss the bottom sheet.
            return;
        }
        AccountInfo childAccountInfo =
                identityManager.findExtendedAccountInfoByEmailAddress(childEmail);

        String childNameProperty = childEmail;
        if (childAccountInfo != null && !childAccountInfo.getGivenName().isEmpty()) {
            childNameProperty = childAccountInfo.getGivenName();
        }
        mModel.set(WebsiteApprovalProperties.CHILD_NAME, childNameProperty);
    }
}
