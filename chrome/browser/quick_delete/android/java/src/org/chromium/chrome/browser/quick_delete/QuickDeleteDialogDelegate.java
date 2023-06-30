// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.quick_delete;

import android.content.Context;
import android.text.SpannableString;
import android.text.method.LinkMovementMethod;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.TextView;

import androidx.annotation.NonNull;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.components.browser_ui.widget.text.TemplatePreservingTextView;
import org.chromium.components.browser_ui.widget.text.TextViewWithCompoundDrawables;
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
    private PropertyModel mModalDialogPropertyModel;

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
     * Stores the data needed for the dialog.
     *
     * TODO(crbug.com/1412087): Update the class to include domain related data.
     */
    static class QuickDeleteDialogData {
        private final int mTabsToCloseCount;

        /**
         * @param tabsToCloseCount the count of tabs that the user visited within range and will
         *         be closed with the deletion.
         */
        QuickDeleteDialogData(int tabsToCloseCount) {
            mTabsToCloseCount = tabsToCloseCount;
        }

        @VisibleForTesting
        QuickDeleteDialogData() {
            mTabsToCloseCount = 0;
        }
    }

    /**
     * @param context               The associated {@link Context}.
     * @param modalDialogManager    A {@link ModalDialogManager} responsible for showing the quick
     *                              delete modal dialog.
     * @param onDismissCallback     A {@link Callback} that will be notified when the user
     *                              confirms or
     *                              cancels the deletion;
     * @param tabModelSelector      {@link TabModelSelector} to use for opening the links in search
     *                              history disambiguation notice.
     */
    QuickDeleteDialogDelegate(@NonNull Context context,
            @NonNull ModalDialogManager modalDialogManager,
            @NonNull Callback<Integer> onDismissCallback,
            @NonNull TabModelSelector tabModelSelector) {
        mContext = context;
        mModalDialogManager = modalDialogManager;
        mOnDismissCallback = onDismissCallback;
        mTabModelSelector = tabModelSelector;
    }

    /**
     * A method to create the dialog attributes for the quick delete dialog.
     * @param quickDeleteDialogData The dialog related data.
     */
    private PropertyModel createQuickDeleteDialogProperty(
            @NonNull QuickDeleteDialogData quickDeleteDialogData) {
        View quickDeleteDialogView =
                LayoutInflater.from(mContext).inflate(R.layout.quick_delete_dialog, /*root=*/null);

        // Add the browsing history row.
        ViewGroup quickDeleteHistoryRow =
                quickDeleteDialogView.findViewById(R.id.quick_delete_history_row);
        addBrowsingHistoryRow(quickDeleteHistoryRow);

        // Add the tabs close row.
        TextViewWithCompoundDrawables quickDeleteTabsCloseRow =
                quickDeleteDialogView.findViewById(R.id.quick_delete_tabs_close_row);
        addTabsCloseRowIfAvailable(quickDeleteTabsCloseRow, quickDeleteDialogData);

        // Add search history disambiguation notice.
        TextViewWithClickableSpans searchHistoryDisambiguation =
                quickDeleteDialogView.findViewById(R.id.search_history_disambiguation);
        addSearchHistoryDisambiguationTextIfRequired(searchHistoryDisambiguation);

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
     * Sets up the browsing history row in the dialog.
     * @param row The browsing history row view.
     *
     * TODO(crbug.com/1412087): Update the method name when the logic is in place.
     */
    private void addBrowsingHistoryRow(@NonNull ViewGroup row) {
        // TODO(crbug.com/1412087): Supply the right domain and count values once the logic is in
        // place and hide the row if there are no domains visited within the time range.
        TemplatePreservingTextView title = row.findViewById(R.id.quick_delete_history_row_title);
        TextView subtitle = row.findViewById(R.id.quick_delete_history_row_subtitle);

        String lastVisitedDomain = "figma.com";
        String domainCountText = mContext.getResources().getQuantityString(
                R.plurals.quick_delete_dialog_browsing_history_domain_count_text, 1);

        String browsingHistoryRowTitleTemplate = "%s " + domainCountText;

        title.setTemplate(browsingHistoryRowTitleTemplate);
        title.setText(lastVisitedDomain);

        row.setVisibility(View.VISIBLE);

        // TODO(crbug.com/1412087): Check if history sync is enabled before showing this.
        subtitle.setVisibility(View.VISIBLE);
    }

    /**
     * Checks whether the user has any tabs in range to close and updates the views accordingly.
     * @param row The tabs close row view.
     * @param data The dialog related data.
     */
    private void addTabsCloseRowIfAvailable(
            @NonNull TextViewWithCompoundDrawables row, @NonNull QuickDeleteDialogData data) {
        if (data.mTabsToCloseCount > 0) {
            String tabDescription = mContext.getResources().getQuantityString(
                    R.plurals.quick_delete_dialog_tabs_closed_text, data.mTabsToCloseCount,
                    data.mTabsToCloseCount);
            row.setText(tabDescription);
        } else {
            row.setVisibility(View.GONE);
        }
    }

    /**
     * Checks whether the user is signed in and updates the views accordingly.
     * @param text The search history disambiguation text.
     */
    private void addSearchHistoryDisambiguationTextIfRequired(
            @NonNull TextViewWithClickableSpans text) {
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
            text.setText(searchHistoryText);
            text.setMovementMethod(LinkMovementMethod.getInstance());
            text.setVisibility(View.VISIBLE);
        }
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
     * @param quickDeleteDialogData The dialog related data.
     */
    void showDialog(@NonNull QuickDeleteDialogData quickDeleteDialogData) {
        mModalDialogPropertyModel = createQuickDeleteDialogProperty(quickDeleteDialogData);
        mModalDialogManager.showDialog(
                mModalDialogPropertyModel, ModalDialogManager.ModalDialogType.APP);
    }
}
