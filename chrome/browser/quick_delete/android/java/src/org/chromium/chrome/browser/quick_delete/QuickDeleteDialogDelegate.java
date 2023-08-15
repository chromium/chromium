// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.quick_delete;

import static org.chromium.chrome.browser.browsing_data.TimePeriodUtils.getTimePeriodSpinnerOptions;

import android.content.Context;
import android.text.SpannableString;
import android.text.method.LinkMovementMethod;
import android.view.View;
import android.view.ViewGroup;
import android.widget.AdapterView;
import android.widget.ArrayAdapter;
import android.widget.Spinner;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.base.Callback;
import org.chromium.chrome.browser.browsing_data.TimePeriod;
import org.chromium.chrome.browser.browsing_data.TimePeriodUtils.TimePeriodSpinnerOption;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.components.browser_ui.settings.SettingsLauncher;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.text.NoUnderlineClickableSpan;
import org.chromium.ui.text.SpanApplier;
import org.chromium.ui.widget.ButtonCompat;
import org.chromium.ui.widget.TextViewWithClickableSpans;

/**
 * A delegate responsible for providing logic around the quick delete modal dialog.
 */
class QuickDeleteDialogDelegate {
    /**
     * An observer for changes made to the spinner in the quick delete dialog.
     */
    interface TimePeriodChangeObserver {
        /**
         * @param timePeriod The new {@link TimePeriod} selected by the user.
         */
        void onTimePeriodChanged(@TimePeriod int timePeriod);
    }

    private final @NonNull ModalDialogManager mModalDialogManager;
    private final @NonNull Context mContext;
    private final @NonNull View mQuickDeleteView;
    private final @NonNull Callback<Integer> mOnDismissCallback;
    private final @NonNull TabModelSelector mTabModelSelector;
    // TODO(crbug.com/1412087): Remove this and instead specify ON_MORE_OPTIONS_CLICKED property and
    // bind the launcher from the {@link QuickDeleteController}.
    private final @NonNull SettingsLauncher mSettingsLauncher;
    private final @NonNull TimePeriodChangeObserver mTimePeriodChangeObserver;
    /**
     * The {@link PropertyModel} of the underlying dialog where the quick dialog view would be
     * shown.
     */
    private PropertyModel mModalDialogPropertyModel;
    private TimePeriodSpinnerOption mCurrentTimePeriodOption;

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
     * @param context            The associated {@link Context}.
     * @param quickDeleteView    {@link View} of the quick delete.
     * @param modalDialogManager A {@link ModalDialogManager} responsible for showing the quick
     *                           delete modal dialog.
     * @param onDismissCallback  A {@link Callback} that will be notified when the user
     *                           confirms or
     *                           cancels the deletion;
     * @param tabModelSelector   {@link TabModelSelector} to use for opening the links in search
     *                           history disambiguation notice.
     * @param settingsLauncher   @link SettingsLauncher} used to launch the Clear browsing data
     *                           settings fragment.
     * @param timePeriodChangeObserver {@link TimePeriodChangeObserver} which would be notified when
     *         the spinner is toggled.
     */
    QuickDeleteDialogDelegate(@NonNull Context context, @NonNull View quickDeleteView,
            @NonNull ModalDialogManager modalDialogManager,
            @NonNull Callback<Integer> onDismissCallback,
            @NonNull TabModelSelector tabModelSelector, @NonNull SettingsLauncher settingsLauncher,
            @NonNull TimePeriodChangeObserver timePeriodChangeObserver) {
        mContext = context;
        mQuickDeleteView = quickDeleteView;
        mModalDialogManager = modalDialogManager;
        mOnDismissCallback = onDismissCallback;
        mTabModelSelector = tabModelSelector;
        mSettingsLauncher = settingsLauncher;
        mTimePeriodChangeObserver = timePeriodChangeObserver;

        mCurrentTimePeriodOption = new TimePeriodSpinnerOption(TimePeriod.LAST_15_MINUTES,
                mContext.getString(R.string.clear_browsing_data_tab_period_15_minutes));
    }

    /**
     * A method to create the dialog attributes for the quick delete dialog.
     */
    private PropertyModel createQuickDeleteDialogProperty() {
        // Update Spinner
        Spinner quickDeleteSpinner = mQuickDeleteView.findViewById(R.id.quick_delete_spinner);
        updateSpinner(quickDeleteSpinner);

        // Update the "More options" button.
        ButtonCompat moreOptionsView =
                mQuickDeleteView.findViewById(R.id.quick_delete_more_options);
        updateMoreOptions(moreOptionsView);

        // Update search history text
        setUpSearchHistoryText();

        return new PropertyModel.Builder(ModalDialogProperties.ALL_KEYS)
                .with(ModalDialogProperties.CONTROLLER, mModalDialogController)
                .with(ModalDialogProperties.CUSTOM_VIEW, mQuickDeleteView)
                .with(ModalDialogProperties.POSITIVE_BUTTON_TEXT,
                        mContext.getString(R.string.clear_data_delete))
                .with(ModalDialogProperties.NEGATIVE_BUTTON_TEXT,
                        mContext.getString(R.string.cancel))
                .with(ModalDialogProperties.CANCEL_ON_TOUCH_OUTSIDE, true)
                .with(ModalDialogProperties.BUTTON_STYLES,
                        ModalDialogProperties.ButtonStyles.PRIMARY_FILLED_NEGATIVE_OUTLINE)
                .build();
    }

    /**
     * Sets up the {@link Spinner} shown in the dialog.
     *
     * @param quickDeleteSpinner The quick delete {@link Spinner} which would be shown in the
     *                           dialog.
     */
    private void updateSpinner(@NonNull Spinner quickDeleteSpinner) {
        TimePeriodSpinnerOption[] options = getTimePeriodSpinnerOptions(mContext);
        ArrayAdapter<TimePeriodSpinnerOption> adapter = new ArrayAdapter<>(
                mContext, android.R.layout.simple_spinner_dropdown_item, options) {
            @NonNull
            @Override
            public View getView(
                    int position, @Nullable View convertView, @NonNull ViewGroup parent) {
                View view = super.getView(position, convertView, parent);
                view.setPadding(0, 0, 0, 0);
                return view;
            }
        };

        quickDeleteSpinner.setAdapter(adapter);
        quickDeleteSpinner.setOnItemSelectedListener(new AdapterView.OnItemSelectedListener() {
            @Override
            public void onItemSelected(
                    AdapterView<?> adapterView, View view, int position, long id) {
                TimePeriodSpinnerOption item =
                        (TimePeriodSpinnerOption) adapterView.getItemAtPosition(position);
                mCurrentTimePeriodOption = item;
                mTimePeriodChangeObserver.onTimePeriodChanged(
                        mCurrentTimePeriodOption.getTimePeriod());
            }

            @Override
            public void onNothingSelected(AdapterView<?> adapterView) {
                // Revert back to default time.
                mCurrentTimePeriodOption = new TimePeriodSpinnerOption(TimePeriod.LAST_15_MINUTES,
                        mContext.getString(R.string.clear_browsing_data_tab_period_15_minutes));
            }
        });
    }

    private void setUpSearchHistoryText() {
        TextViewWithClickableSpans text =
                mQuickDeleteView.findViewById(R.id.search_history_disambiguation);
        final SpannableString searchHistoryText = SpanApplier.applySpans(
                mContext.getString(R.string.quick_delete_dialog_search_history_disambiguation_text),
                new SpanApplier.SpanInfo("<link1>", "</link1>",
                        new NoUnderlineClickableSpan(mContext,
                                (widget)
                                        -> openUrlInNewTab(
                                                UrlConstants.GOOGLE_SEARCH_HISTORY_URL_IN_QD))),
                new SpanApplier.SpanInfo("<link2>", "</link2>",
                        new NoUnderlineClickableSpan(mContext,
                                (widget) -> openUrlInNewTab(UrlConstants.MY_ACTIVITY_URL_IN_QD))));
        text.setText(searchHistoryText);
        text.setMovementMethod(LinkMovementMethod.getInstance());
    }

    private void updateMoreOptions(@NonNull ButtonCompat moreOptionsView) {
        moreOptionsView.setOnClickListener(view
                -> mSettingsLauncher.launchSettingsActivity(mContext,
                        SettingsLauncher.SettingsFragment.CLEAR_BROWSING_DATA_ADVANCED_PAGE));
    }

    /**
     * Opens a url in a new non-incognito tab and dismisses the dialog.
     *
     * @param url The URL of the page to load, either GOOGLE_SEARCH_HISTORY_URL_IN_QD or
     *            MY_ACTIVITY_URL_IN_QD.
     */
    private void openUrlInNewTab(final String url) {
        mTabModelSelector.openNewTab(new LoadUrlParams(url), TabLaunchType.FROM_CHROME_UI,
                mTabModelSelector.getCurrentTab(), false);
        mModalDialogManager.dismissDialog(
                mModalDialogPropertyModel, DialogDismissalCause.ACTION_ON_CONTENT);
    }

    /**
     * Shows the Quick delete dialog.
     */
    void showDialog() {
        mModalDialogPropertyModel = createQuickDeleteDialogProperty();
        mModalDialogManager.showDialog(
                mModalDialogPropertyModel, ModalDialogManager.ModalDialogType.APP);
    }
}
