// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.touch_to_fill;

import static org.chromium.chrome.browser.password_manager.PasswordManagerHelper.usesUnifiedPasswordManagerBranding;

import android.content.Context;

import androidx.annotation.VisibleForTesting;

import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.touch_to_fill.common.BottomSheetFocusHelper;
import org.chromium.chrome.browser.touch_to_fill.data.Credential;
import org.chromium.chrome.browser.touch_to_fill.data.WebAuthnCredential;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.favicon.LargeIconBridge;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;
import org.chromium.url.GURL;

import java.util.List;

/**
 * Creates the TouchToFill component. TouchToFill uses a bottom sheet to let the
 * user select a set of credentials and fills it into the focused form.
 */
public class TouchToFillCoordinator implements TouchToFillComponent {
    private final TouchToFillMediator mMediator = new TouchToFillMediator();
    private final PropertyModel mModel =
            TouchToFillProperties.createDefaultModel(mMediator::onDismissed);

    @Override
    public void initialize(Context context, BottomSheetController sheetController,
            TouchToFillComponent.Delegate delegate, BottomSheetFocusHelper bottomSheetFocusHelper) {
        mMediator.initialize(context, delegate, mModel,
                new LargeIconBridge(Profile.getLastUsedRegularProfile()),
                context.getResources().getDimensionPixelSize(usesUnifiedPasswordManagerBranding()
                                ? R.dimen.touch_to_fill_favicon_size_modern
                                : R.dimen.touch_to_fill_favicon_size),
                bottomSheetFocusHelper);
        setUpModelChangeProcessors(mModel, new TouchToFillView(context, sheetController));
    }

    @Override
    public void showCredentials(GURL url, boolean isOriginSecure,
            List<WebAuthnCredential> webAuthnCredentials, List<Credential> credentials,
            boolean triggerSubmission, boolean managePasskeysHidesPasswords) {
        mMediator.showCredentials(url, isOriginSecure, webAuthnCredentials, credentials,
                triggerSubmission, managePasskeysHidesPasswords);
    }

    /**
     * Connects the given model with the given view using Model Change Processors.
     * @param model A {@link PropertyModel} built with {@link TouchToFillProperties}.
     * @param view A {@link TouchToFillView}.
     */
    @VisibleForTesting
    static void setUpModelChangeProcessors(PropertyModel model, TouchToFillView view) {
        PropertyModelChangeProcessor.create(
                model, view, TouchToFillViewBinder::bindTouchToFillView);
    }
}
