// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.quick_delete;

import android.content.Context;
import android.text.SpannableString;
import android.text.method.LinkMovementMethod;
import android.view.LayoutInflater;
import android.view.View;

import androidx.annotation.NonNull;

import org.chromium.base.Callback;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.components.signin.identitymanager.ConsentLevel;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.text.NoUnderlineClickableSpan;
import org.chromium.ui.text.SpanApplier;
import org.chromium.ui.widget.TextViewWithClickableSpans;

/**
 * A delegate responsible for providing logic around the quick delete modal dialog.
 */
class QuickDeleteDialogDelegate {
    private final @NonNull ModalDialogManager mModalDialogManager;
    private final @NonNull Context mContext;
    private final @NonNull Callback<Integer> mOnDismissCallback;
    private final @NonNull TabModelSelector mTabModelSelector;
    /**The {@link PropertyModel} of the underlying dialog where the quick dialog view would be
     * shown.*/
    private final PropertyModel mModalDialogPropertyModel;

    /**
     * The modal dialog controller to detect events on the dialog.
     */
    private final ModalDialogProperties.Controller mModalDialogController =
            new ModalDialogProperties.Controller() {
                @Override
                public void onClick(PropertyModel model, int buttonType) {
                    if (buttonType == ModalDialogProperties.ButtonType.POSITIVE) {
                        mModalDialogManager.dismissDialog(mModalDialogPropertyModel,
                                DialogDismissalCause.POSITIVE_BUTTON_CLICKED);
                    } else if (buttonType == ModalDialogProperties.ButtonType.NEGATIVE) {
                        mModalDialogManager.dismissDialog(mModalDialogPropertyModel,
                                DialogDismissalCause.NEGATIVE_BUTTON_CLICKED);
                    }
                }

                @Override
                public void onDismiss(PropertyModel model, int dismissalCause) {
                    mOnDismissCallback.onResult(dismissalCause);
                }
            };

    /**
     * @param context The associated {@link Context}.
     * @param modalDialogManager A {@link ModalDialogManager} responsible for showing the quick
     *         delete modal dialog.
     * @param onDismissCallback A {@link Callback} that will be notified when the user confirms or
     *         cancels the deletion;
     * @param tabModelSelector {@link TabModelSelector} to use for opening the links in search
     *         history disambiguation notice.
     */
    QuickDeleteDialogDelegate(@NonNull Context context,
            @NonNull ModalDialogManager modalDialogManager,
            @NonNull Callback<Integer> onDismissCallback,
            @NonNull TabModelSelector tabModelSelector) {
        mContext = context;
        mModalDialogManager = modalDialogManager;
        mOnDismissCallback = onDismissCallback;
        mTabModelSelector = tabModelSelector;
        mModalDialogPropertyModel = createQuickDeleteDialogProperty();
    }

    /**
     * A method to create the dialog attributes for the quick delete dialog.
     */
    private PropertyModel createQuickDeleteDialogProperty() {
        View quickDeleteDialogView =
                LayoutInflater.from(mContext).inflate(R.layout.quick_delete_dialog, /*root=*/null);

        TextViewWithClickableSpans searchHistoryDisambiguation =
                quickDeleteDialogView.findViewById(R.id.search_history_disambiguation);

        if (isSignedIn()) {
            // Add search history and other activity links to search history disambiguation notice
            // in the dialog.
            final SpannableString searchHistoryText = SpanApplier.applySpans(
                    mContext.getString(
                            R.string.quick_delete_dialog_search_history_disambiguation_text),
                    new SpanApplier.SpanInfo("<link1>", "</link1>",
                            new NoUnderlineClickableSpan(mContext,
                                    (widget)
                                            -> openUrlInNewTab(
                                                    UrlConstants.GOOGLE_SEARCH_HISTORY_URL_IN_QD))),
                    new SpanApplier.SpanInfo("<link2>", "</link2>",
                            new NoUnderlineClickableSpan(mContext,
                                    (widget)
                                            -> openUrlInNewTab(
                                                    UrlConstants.MY_ACTIVITY_URL_IN_QD))));
            searchHistoryDisambiguation.setText(searchHistoryText);
            searchHistoryDisambiguation.setMovementMethod(LinkMovementMethod.getInstance());
            searchHistoryDisambiguation.setVisibility(View.VISIBLE);
        }

        return new PropertyModel.Builder(ModalDialogProperties.ALL_KEYS)
                .with(ModalDialogProperties.CONTROLLER, mModalDialogController)
                .with(ModalDialogProperties.TITLE,
                        mContext.getString(R.string.quick_delete_dialog_title))
                .with(ModalDialogProperties.CUSTOM_VIEW, quickDeleteDialogView)
                .with(ModalDialogProperties.POSITIVE_BUTTON_TEXT,
                        mContext.getString(R.string.delete))
                .with(ModalDialogProperties.NEGATIVE_BUTTON_TEXT,
                        mContext.getString(R.string.cancel))
                .with(ModalDialogProperties.CANCEL_ON_TOUCH_OUTSIDE, true)
                .with(ModalDialogProperties.BUTTON_STYLES,
                        ModalDialogProperties.ButtonStyles.PRIMARY_FILLED_NEGATIVE_OUTLINE)
                .build();
    }

    /**
     * Opens a url in a new non-incognito tab and dismisses the dialog.
     * @param url The URL of the page to load, either GOOGLE_SEARCH_HISTORY_URL_IN_QD or
     *         MY_ACTIVITY_URL_IN_QD.
     */
    private void openUrlInNewTab(final String url) {
        mTabModelSelector.openNewTab(new LoadUrlParams(url), TabLaunchType.FROM_CHROME_UI,
                mTabModelSelector.getCurrentTab(), false);
        mModalDialogManager.dismissDialog(
                mModalDialogPropertyModel, DialogDismissalCause.ACTION_ON_CONTENT);
    }

    /**
     * @return A boolean indicating whether the user is signed in or not.
     */
    private boolean isSignedIn() {
        Profile profile = mTabModelSelector.getCurrentModel().getProfile();
        return IdentityServicesProvider.get().getIdentityManager(profile).hasPrimaryAccount(
                ConsentLevel.SIGNIN);
    }

    /**
     * Shows the Quick delete dialog.
     */
    void showDialog() {
        mModalDialogManager.showDialog(
                mModalDialogPropertyModel, ModalDialogManager.ModalDialogType.APP);
    }
}
