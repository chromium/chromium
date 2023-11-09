// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.touch_to_fill;

import static org.chromium.chrome.browser.touch_to_fill.TouchToFillProperties.CredentialProperties.CREDENTIAL;
import static org.chromium.chrome.browser.touch_to_fill.TouchToFillProperties.CredentialProperties.FAVICON_OR_FALLBACK;
import static org.chromium.chrome.browser.touch_to_fill.TouchToFillProperties.CredentialProperties.FORMATTED_ORIGIN;
import static org.chromium.chrome.browser.touch_to_fill.TouchToFillProperties.CredentialProperties.ITEM_COLLECTION_INFO;
import static org.chromium.chrome.browser.touch_to_fill.TouchToFillProperties.CredentialProperties.ON_CLICK_LISTENER;
import static org.chromium.chrome.browser.touch_to_fill.TouchToFillProperties.CredentialProperties.SHOW_SUBMIT_BUTTON;
import static org.chromium.chrome.browser.touch_to_fill.TouchToFillProperties.FooterProperties.MANAGE_BUTTON_TEXT;
import static org.chromium.chrome.browser.touch_to_fill.TouchToFillProperties.FooterProperties.ON_CLICK_HYBRID;
import static org.chromium.chrome.browser.touch_to_fill.TouchToFillProperties.FooterProperties.ON_CLICK_MANAGE;
import static org.chromium.chrome.browser.touch_to_fill.TouchToFillProperties.FooterProperties.SHOW_HYBRID;
import static org.chromium.chrome.browser.touch_to_fill.TouchToFillProperties.HeaderProperties.FORMATTED_URL;
import static org.chromium.chrome.browser.touch_to_fill.TouchToFillProperties.HeaderProperties.IMAGE_DRAWABLE_ID;
import static org.chromium.chrome.browser.touch_to_fill.TouchToFillProperties.HeaderProperties.ORIGIN_SECURE;
import static org.chromium.chrome.browser.touch_to_fill.TouchToFillProperties.HeaderProperties.SHOW_SUBMIT_SUBTITLE;
import static org.chromium.chrome.browser.touch_to_fill.TouchToFillProperties.HeaderProperties.TITLE;
import static org.chromium.chrome.browser.touch_to_fill.TouchToFillProperties.SHEET_ITEMS;
import static org.chromium.chrome.browser.touch_to_fill.TouchToFillProperties.VISIBLE;
import static org.chromium.chrome.browser.touch_to_fill.TouchToFillProperties.WebAuthnCredentialProperties.ON_WEBAUTHN_CLICK_LISTENER;
import static org.chromium.chrome.browser.touch_to_fill.TouchToFillProperties.WebAuthnCredentialProperties.SHOW_WEBAUTHN_SUBMIT_BUTTON;
import static org.chromium.chrome.browser.touch_to_fill.TouchToFillProperties.WebAuthnCredentialProperties.WEBAUTHN_CREDENTIAL;
import static org.chromium.chrome.browser.touch_to_fill.TouchToFillProperties.WebAuthnCredentialProperties.WEBAUTHN_FAVICON_OR_FALLBACK;
import static org.chromium.chrome.browser.touch_to_fill.TouchToFillProperties.WebAuthnCredentialProperties.WEBAUTHN_ITEM_COLLECTION_INFO;

import android.content.Context;

import androidx.annotation.Px;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.browser.password_manager.PasswordManagerResourceProviderFactory;
import org.chromium.chrome.browser.touch_to_fill.TouchToFillComponent.UserAction;
import org.chromium.chrome.browser.touch_to_fill.TouchToFillProperties.CredentialProperties;
import org.chromium.chrome.browser.touch_to_fill.TouchToFillProperties.FaviconOrFallback;
import org.chromium.chrome.browser.touch_to_fill.TouchToFillProperties.FooterProperties;
import org.chromium.chrome.browser.touch_to_fill.TouchToFillProperties.HeaderProperties;
import org.chromium.chrome.browser.touch_to_fill.TouchToFillProperties.MorePasskeysProperties;
import org.chromium.chrome.browser.touch_to_fill.TouchToFillProperties.WebAuthnCredentialProperties;
import org.chromium.chrome.browser.touch_to_fill.common.BottomSheetFocusHelper;
import org.chromium.chrome.browser.touch_to_fill.common.FillableItemCollectionInfo;
import org.chromium.chrome.browser.touch_to_fill.data.Credential;
import org.chromium.chrome.browser.touch_to_fill.data.WebAuthnCredential;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.StateChangeReason;
import org.chromium.components.embedder_support.util.Origin;
import org.chromium.components.favicon.LargeIconBridge;
import org.chromium.components.favicon.LargeIconBridge.LargeIconCallback;
import org.chromium.components.url_formatter.SchemeDisplay;
import org.chromium.components.url_formatter.UrlFormatter;
import org.chromium.ui.modelutil.ListModel;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.url.GURL;

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

    private Context mContext;
    private TouchToFillComponent.Delegate mDelegate;
    private PropertyModel mModel;
    private LargeIconBridge mLargeIconBridge;
    private @Px int mDesiredIconSize;
    private List<WebAuthnCredential> mWebAuthnCredentials;
    private List<Credential> mCredentials;
    private boolean mManagePasskeysHidesPasswords;
    private BottomSheetFocusHelper mBottomSheetFocusHelper;

    void initialize(Context context, TouchToFillComponent.Delegate delegate, PropertyModel model,
            LargeIconBridge largeIconBridge, @Px int desiredIconSize,
            BottomSheetFocusHelper bottomSheetFocusHelper) {
        assert delegate != null;
        mContext = context;
        mDelegate = delegate;
        mModel = model;
        mLargeIconBridge = largeIconBridge;
        mDesiredIconSize = desiredIconSize;
        mBottomSheetFocusHelper = bottomSheetFocusHelper;
    }

    void showCredentials(GURL url, boolean isOriginSecure,
            List<WebAuthnCredential> webAuthnCredentials, List<Credential> credentials,
            boolean showMorePasskeys, boolean triggerSubmission,
            boolean managePasskeysHidesPasswords, boolean showHybridPasskeyOption) {
        assert credentials != null;

        mManagePasskeysHidesPasswords = managePasskeysHidesPasswords;

        ListModel<ListItem> sheetItems = mModel.get(SHEET_ITEMS);
        sheetItems.clear();

        sheetItems.add(new ListItem(TouchToFillProperties.ItemType.HEADER,
                new PropertyModel.Builder(HeaderProperties.ALL_KEYS)
                        .with(TITLE, getTitle(webAuthnCredentials, credentials))
                        .with(FORMATTED_URL,
                                UrlFormatter.formatUrlForSecurityDisplay(
                                        url, SchemeDisplay.OMIT_HTTP_AND_HTTPS))
                        .with(ORIGIN_SECURE, isOriginSecure)
                        .with(SHOW_SUBMIT_SUBTITLE, triggerSubmission)
                        // TODO(crbug.com/1471888): Use the TTF resource provider instead and use a
                        // 32dp icon.
                        .with(IMAGE_DRAWABLE_ID,
                                PasswordManagerResourceProviderFactory.create()
                                        .getPasswordManagerIcon())
                        .build()));

        int fillableItemsTotal = credentials.size() + webAuthnCredentials.size();
        int fillableItemPosition = 0;

        mWebAuthnCredentials = webAuthnCredentials;
        for (WebAuthnCredential credential : webAuthnCredentials) {
            final PropertyModel model = createWebAuthnModel(credential,
                    new FillableItemCollectionInfo(++fillableItemPosition, fillableItemsTotal));
            sheetItems.add(new ListItem(TouchToFillProperties.ItemType.WEBAUTHN_CREDENTIAL, model));
            if (shouldCreateConfirmationButton(
                        credentials, webAuthnCredentials, showMorePasskeys)) {
                sheetItems.add(new ListItem(TouchToFillProperties.ItemType.FILL_BUTTON, model));
            }
            requestWebAuthnIconOrFallbackImage(model, url);
        }

        mCredentials = credentials;
        for (Credential credential : credentials) {
            final PropertyModel model = createModel(credential, triggerSubmission,
                    new FillableItemCollectionInfo(++fillableItemPosition, fillableItemsTotal));
            sheetItems.add(new ListItem(TouchToFillProperties.ItemType.CREDENTIAL, model));
            if (shouldCreateConfirmationButton(
                        credentials, webAuthnCredentials, showMorePasskeys)) {
                sheetItems.add(new ListItem(TouchToFillProperties.ItemType.FILL_BUTTON, model));
            }
            requestIconOrFallbackImage(model, url);
        }

        if (showMorePasskeys) {
            String morePasskeyTitle =
                    webAuthnCredentials.size() == 0
                            ? mContext.getString(R.string.touch_to_fill_select_passkey)
                            : mContext.getString(R.string.touch_to_fill_more_passkeys);
            sheetItems.add(
                    new ListItem(
                            TouchToFillProperties.ItemType.MORE_PASSKEYS,
                            new PropertyModel.Builder(MorePasskeysProperties.ALL_KEYS)
                                    .with(
                                            MorePasskeysProperties.ON_CLICK,
                                            this::onSelectedMorePasskeys)
                                    .with(MorePasskeysProperties.TITLE, morePasskeyTitle)
                                    .build()));
        }

        sheetItems.add(new ListItem(TouchToFillProperties.ItemType.FOOTER,
                new PropertyModel.Builder(FooterProperties.ALL_KEYS)
                        .with(ON_CLICK_MANAGE, this::onManagePasswordSelected)
                        .with(MANAGE_BUTTON_TEXT,
                                getManageButtonText(credentials, webAuthnCredentials))
                        .with(ON_CLICK_HYBRID, this::onHybridSignInSelected)
                        .with(SHOW_HYBRID, showHybridPasskeyOption)
                        .build()));

        mBottomSheetFocusHelper.registerForOneTimeUse();
        mModel.set(VISIBLE, true);
    }

    private String getTitle(
            List<WebAuthnCredential> webAuthnCredentials, List<Credential> credentials) {
        if (webAuthnCredentials.size() > 0) {
            return (credentials.size() > 0)
                    ? mContext.getString(R.string.touch_to_fill_sheet_title_password_or_passkey)
                    : mContext.getString(R.string.touch_to_fill_sheet_title_passkey);
        }

        return mContext.getString(R.string.touch_to_fill_sheet_uniform_title);
    }

    private String getManageButtonText(
            List<Credential> credentials, List<WebAuthnCredential> webAuthnCredentials) {
        if (webAuthnCredentials.size() == 0) {
            return mContext.getString(R.string.manage_passwords);
        }

        if (credentials.size() > 0 && !mManagePasskeysHidesPasswords) {
            return mContext.getString(R.string.manage_passwords_and_passkeys);
        }

        return mContext.getString(R.string.manage_passkeys);
    }

    private void requestIconOrFallbackImage(PropertyModel credentialModel, GURL url) {
        Credential credential = credentialModel.get(CREDENTIAL);
        final String iconOrigin = getIconOrigin(credential.getOriginUrl(), url);

        final LargeIconCallback setIcon = (icon, fallbackColor, hasDefaultColor, type) -> {
            credentialModel.set(FAVICON_OR_FALLBACK,
                    new FaviconOrFallback(iconOrigin, icon, fallbackColor, hasDefaultColor, type,
                            mDesiredIconSize));
        };
        final LargeIconCallback setIconOrRetry = (icon, fallbackColor, hasDefaultColor, type) -> {
            if (icon == null && iconOrigin.equals(credential.getOriginUrl())) {
                mLargeIconBridge.getLargeIconForUrl(url, mDesiredIconSize, setIcon);
                return; // Unlikely but retry for exact path if there is no icon for the origin.
            }
            setIcon.onLargeIconAvailable(icon, fallbackColor, hasDefaultColor, type);
        };
        mLargeIconBridge.getLargeIconForStringUrl(iconOrigin, mDesiredIconSize, setIconOrRetry);
    }

    private void requestWebAuthnIconOrFallbackImage(PropertyModel credentialModel, GURL url) {
        // WebAuthn credentials have already been filtered to match the current site's URL.
        final String iconOrigin = url.getSpec();

        final LargeIconCallback setIcon = (icon, fallbackColor, hasDefaultColor, type) -> {
            credentialModel.set(WEBAUTHN_FAVICON_OR_FALLBACK,
                    new FaviconOrFallback(iconOrigin, icon, fallbackColor, hasDefaultColor, type,
                            mDesiredIconSize));
        };
        final LargeIconCallback setIconOrRetry = (icon, fallbackColor, hasDefaultColor, type) -> {
            if (icon == null) {
                mLargeIconBridge.getLargeIconForUrl(url, mDesiredIconSize, setIcon);
                return; // Unlikely but retry for exact path if there is no icon for the origin.
            }
            setIcon.onLargeIconAvailable(icon, fallbackColor, hasDefaultColor, type);
        };
        mLargeIconBridge.getLargeIconForStringUrl(iconOrigin, mDesiredIconSize, setIconOrRetry);
    }

    private String getIconOrigin(String credentialOrigin, GURL siteUrl) {
        final Origin o = Origin.create(credentialOrigin);
        // TODO(crbug.com/1030230): assert o != null as soon as credential Origin must be valid.
        return o != null && !o.uri().isOpaque() ? credentialOrigin : siteUrl.getSpec();
    }

    private void reportCredentialSelection(int userAction, int index) {
        if (mCredentials.size() + mWebAuthnCredentials.size() > 1) {
            // We only record this histogram in case multiple credentials were shown to the user.
            // Otherwise the single credential case where position should always be 0 will dominate
            // the recording.
            RecordHistogram.recordCount100Histogram(UMA_TOUCH_TO_FILL_CREDENTIAL_INDEX, index);
        }
    }

    private void onSelectedCredential(Credential credential) {
        mModel.set(VISIBLE, false);
        reportCredentialSelection(UserAction.SELECT_CREDENTIAL, mCredentials.indexOf(credential));
        mDelegate.onCredentialSelected(credential);
    }

    private void onSelectedWebAuthnCredential(WebAuthnCredential credential) {
        mModel.set(VISIBLE, false);
        // The index assumes WebAuthn credentials are listed after password credentials.
        reportCredentialSelection(UserAction.SELECT_WEBAUTHN_CREDENTIAL,
                mCredentials.size() + mWebAuthnCredentials.indexOf(credential));
        mDelegate.onWebAuthnCredentialSelected(credential);
    }

    private void onSelectedMorePasskeys() {
        mModel.set(VISIBLE, false);
        // TODO(crbug/1474805): add metrics
        mDelegate.onShowMorePasskeysSelected();
    }

    public void onDismissed(@StateChangeReason int reason) {
        if (!mModel.get(VISIBLE)) return; // Dismiss only if not dismissed yet.
        mModel.set(VISIBLE, false);
        RecordHistogram.recordEnumeratedHistogram(UMA_TOUCH_TO_FILL_DISMISSAL_REASON, reason,
                BottomSheetController.StateChangeReason.MAX_VALUE + 1);
        mDelegate.onDismissed();
    }

    private void onManagePasswordSelected() {
        mModel.set(VISIBLE, false);
        boolean passkeysShown = (mWebAuthnCredentials.size() > 0);
        mDelegate.onManagePasswordsSelected(passkeysShown);
    }

    private void onHybridSignInSelected() {
        mModel.set(VISIBLE, false);
        mDelegate.onHybridSignInSelected();
    }

    /**
     * @param credentials The available credentials. Show the confirmation for a lone credential.
     * @return True if a confirmation button should be shown at the end of the bottom sheet.
     */
    private boolean shouldCreateConfirmationButton(List<Credential> credentials,
            List<WebAuthnCredential> webauthnCredentials, boolean shouldShowMorePasskeys) {
        if (shouldShowMorePasskeys) return false;
        return credentials.size() + webauthnCredentials.size() == 1;
    }

    private PropertyModel createModel(Credential credential, boolean triggerSubmission,
            FillableItemCollectionInfo itemCollectionInfo) {
        return new PropertyModel.Builder(CredentialProperties.ALL_KEYS)
                .with(CREDENTIAL, credential)
                .with(ON_CLICK_LISTENER, this::onSelectedCredential)
                .with(FORMATTED_ORIGIN, credential.getDisplayName())
                .with(SHOW_SUBMIT_BUTTON, triggerSubmission)
                .with(ITEM_COLLECTION_INFO, itemCollectionInfo)
                .build();
    }

    private PropertyModel createWebAuthnModel(
            WebAuthnCredential credential, FillableItemCollectionInfo itemCollectionInfo) {
        return new PropertyModel.Builder(WebAuthnCredentialProperties.ALL_KEYS)
                .with(WEBAUTHN_CREDENTIAL, credential)
                .with(ON_WEBAUTHN_CLICK_LISTENER, this::onSelectedWebAuthnCredential)
                .with(SHOW_WEBAUTHN_SUBMIT_BUTTON, false)
                .with(WEBAUTHN_ITEM_COLLECTION_INFO, itemCollectionInfo)
                .build();
    }
}
