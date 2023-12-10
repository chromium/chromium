// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.supervised_user.website_approval;

import android.graphics.Bitmap;

import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetControllerProvider;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;
import org.chromium.url.GURL;

/**
 * Coordinator for the bottom sheet content in the screen which allows a parent to approve or deny
 * a website.
 */
public class WebsiteApprovalCoordinator {
    private final WebsiteApprovalMediator mMediator;

    /** Callback to notify completion of the flow. */
    public interface CompletionCallback {
        /** Called when the parent clicks to approve the website. */
        void onWebsiteApproved();

        /** Called when the parent explicitly clicks to not approve the website. */
        void onWebsiteDenied();
    }

    /**
     * Constructor for the co-ordinator. Callers should then call {@link show()} to display the UI.
     *
     * @param url the full URL for which the request is being made (code in this module is
     *     responsible for displaying the appropriate part of the URL to the user)
     */
    public WebsiteApprovalCoordinator(
            WindowAndroid windowAndroid,
            GURL url,
            CompletionCallback completionCallback,
            Bitmap favicon,
            Profile profile) {
        PropertyModel model =
                new PropertyModel.Builder(WebsiteApprovalProperties.ALL_KEYS)
                        .with(WebsiteApprovalProperties.URL, url)
                        .with(WebsiteApprovalProperties.FAVICON, favicon)
                        .build();

        BottomSheetController bottomSheetController =
                BottomSheetControllerProvider.from(windowAndroid);
        WebsiteApprovalSheetContent sheetContent =
                new WebsiteApprovalSheetContent(windowAndroid.getContext().get());

        PropertyModelChangeProcessor.create(model, sheetContent, WebsiteApprovalViewBinder::bind);

        mMediator =
                new WebsiteApprovalMediator(
                        completionCallback, bottomSheetController, sheetContent, model, profile);
    }

    /** Displays the UI to request parent approval in a new bottom sheet. */
    public void show() {
        mMediator.show();
    }
}
