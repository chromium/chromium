// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.touch_to_fill;

import static org.chromium.chrome.browser.touch_to_fill.TouchToFillProperties.CredentialProperties.CREDENTIAL;
import static org.chromium.chrome.browser.touch_to_fill.TouchToFillProperties.CredentialProperties.FAVICON;
import static org.chromium.chrome.browser.touch_to_fill.TouchToFillProperties.CredentialProperties.FORMATTED_ORIGIN;
import static org.chromium.chrome.browser.touch_to_fill.TouchToFillProperties.CredentialProperties.ON_CLICK_LISTENER;
import static org.chromium.chrome.browser.touch_to_fill.TouchToFillProperties.FIELD_TRIAL_PARAM_SHOW_CONFIRMATION_BUTTON;
import static org.chromium.chrome.browser.touch_to_fill.TouchToFillProperties.HeaderProperties.FORMATTED_URL;
import static org.chromium.chrome.browser.touch_to_fill.TouchToFillProperties.HeaderProperties.ORIGIN_SECURE;
import static org.chromium.chrome.browser.touch_to_fill.TouchToFillProperties.ON_CLICK_MANAGE;
import static org.chromium.chrome.browser.touch_to_fill.TouchToFillProperties.SHEET_ITEMS;
import static org.chromium.chrome.browser.touch_to_fill.TouchToFillProperties.VISIBLE;

import androidx.annotation.Px;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.touch_to_fill.TouchToFillComponent.UserAction;
import org.chromium.chrome.browser.touch_to_fill.TouchToFillProperties.CredentialProperties;
import org.chromium.chrome.browser.touch_to_fill.TouchToFillProperties.HeaderProperties;
import org.chromium.chrome.browser.touch_to_fill.data.Credential;
import org.chromium.chrome.browser.widget.bottomsheet.BottomSheetController;
import org.chromium.chrome.browser.widget.bottomsheet.BottomSheetController.StateChangeReason;
import org.chromium.components.url_formatter.UrlFormatter;
import org.chromium.ui.modelutil.ListModel;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.List;

/**
 * Contains the logic for the TouchToFill component. It sets the state of the model and reacts to
 * events like clicks.
 */
class TouchToFillMediator {
    static final String UMA_TOUCH_TO_FILL_DISMISSAL_REASON =
            "PasswordManager.TouchToFill.DismissalReason";
    static final String UMA_TOUCH_TO_FILL_CREDENTIAL_INDEX =
            "PasswordManager.TouchToFill.CredentialIndex";
    static final String UMA_TOUCH_TO_FILL_USER_ACTION = "PasswordManager.TouchToFill.UserAction";

    private TouchToFillComponent.Delegate mDelegate;
    private PropertyModel mModel;
    private @Px int mDesiredFaviconSize;
    private List<Credential> mCredentials;

    void initialize(TouchToFillComponent.Delegate delegate, PropertyModel model,
            @Px int desiredFaviconSize) {
        assert delegate != null;
        mDelegate = delegate;
        mModel = model;
        mDesiredFaviconSize = desiredFaviconSize;
    }

    void showCredentials(String url, boolean isOriginSecure, List<Credential> credentials) {
        assert credentials != null;
        mModel.set(ON_CLICK_MANAGE, this::onManagePasswordSelected);

        ListModel<ListItem> sheetItems = mModel.get(SHEET_ITEMS);
        sheetItems.clear();

        sheetItems.add(new ListItem(TouchToFillProperties.ItemType.HEADER,
                new PropertyModel.Builder(HeaderProperties.ALL_KEYS)
                        .with(FORMATTED_URL,
                                UrlFormatter.formatUrlForSecurityDisplayOmitScheme(url))
                        .with(ORIGIN_SECURE, isOriginSecure)
                        .build()));

        mCredentials = credentials;
        for (Credential credential : credentials) {
            final PropertyModel model = createCredentialModel(credential);
            sheetItems.add(new ListItem(TouchToFillProperties.ItemType.CREDENTIAL, model));
            mDelegate.fetchFavicon(credential.getOriginUrl(), url, mDesiredFaviconSize,
                    (bitmap) -> model.set(FAVICON, bitmap));
            if (shouldCreateConfirmationButton(credentials)) {
                sheetItems.add(new ListItem(TouchToFillProperties.ItemType.FILL_BUTTON,
                        createCredentialModel(credential)));
            }
        }
        mModel.set(VISIBLE, true);
    }

    private void onSelectedCredential(Credential credential) {
        mModel.set(VISIBLE, false);
        if (mCredentials.size() > 1) {
            // We only record this histogram in case multiple credentials were shown to the user.
            // Otherwise the single credential case where position should always be 0 will dominate
            // the recording.
            RecordHistogram.recordCount100Histogram(
                    UMA_TOUCH_TO_FILL_CREDENTIAL_INDEX, mCredentials.indexOf(credential));
        }

        RecordHistogram.recordEnumeratedHistogram(UMA_TOUCH_TO_FILL_USER_ACTION,
                UserAction.SELECT_CREDENTIAL, UserAction.MAX_VALUE + 1);
        mDelegate.onCredentialSelected(credential);
    }

    public void onDismissed(@StateChangeReason int reason) {
        if (!mModel.get(VISIBLE)) return; // Dismiss only if not dismissed yet.
        mModel.set(VISIBLE, false);
        RecordHistogram.recordEnumeratedHistogram(UMA_TOUCH_TO_FILL_DISMISSAL_REASON, reason,
                BottomSheetController.StateChangeReason.MAX_VALUE + 1);
        RecordHistogram.recordEnumeratedHistogram(
                UMA_TOUCH_TO_FILL_USER_ACTION, UserAction.DISMISS, UserAction.MAX_VALUE + 1);
        mDelegate.onDismissed();
    }

    private void onManagePasswordSelected() {
        mModel.set(VISIBLE, false);
        RecordHistogram.recordEnumeratedHistogram(UMA_TOUCH_TO_FILL_USER_ACTION,
                UserAction.SELECT_MANAGE_PASSWORDS, UserAction.MAX_VALUE + 1);
        mDelegate.onManagePasswordsSelected();
    }

    /**
     * @param credentials The available credentials. Show the confirmation for a lone credential.
     * @return True if a confirmation button should be shown at the end of the bottom sheet.
     */
    private boolean shouldCreateConfirmationButton(List<Credential> credentials) {
        return credentials.size() == 1
                && ChromeFeatureList.getFieldTrialParamByFeatureAsBoolean(
                        ChromeFeatureList.TOUCH_TO_FILL_ANDROID,
                        FIELD_TRIAL_PARAM_SHOW_CONFIRMATION_BUTTON, false);
    }

    private PropertyModel createCredentialModel(Credential credential) {
        return new PropertyModel.Builder(CredentialProperties.ALL_KEYS)
                .with(CREDENTIAL, credential)
                .with(ON_CLICK_LISTENER, this::onSelectedCredential)
                .with(FORMATTED_ORIGIN,
                        UrlFormatter.formatUrlForDisplayOmitScheme(credential.getOriginUrl()))
                .build();
    }
}
