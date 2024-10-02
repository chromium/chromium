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
import org.chromium.chrome.browser.settings.SettingsNavigationFactory;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.components.browser_ui.settings.SettingsNavigation;
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

import java.util.Objects;

/** A delegate responsible for providing logic around the quick delete modal dialog. */
class QuickDeleteDialogDelegate {
    /** An observer for changes made to the spinner in the quick delete dialog. */
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
    private final @NonNull TimePeriodChangeObserver mTimePeriodChangeObserver;

    /**
     * The {@link PropertyModel} of the underlying dialog where the quick dialog view would be
     * shown.
     */
    private PropertyModel mModalDialogPropertyModel;

    private TimePeriodSpinnerOption mCurrentTimePeriodOption;

    /** The modal dialog controller to detect events on the dialog. */
    private final ModalDialogProperties.Controller mModalDialogController =
            new ModalDialogProperties.Controller() {
                @Override
                public void onClick(PropertyModel model, int buttonType) {
                    if (buttonType == ModalDialogProperties.ButtonType.POSITIVE) {
                        mModalDialogManager.dismissDialog(
                                mModalDialogPropertyModel,
                                DialogDismissalCause.POSITIVE_BUTTON_CLICKED);
                    } else if (buttonType == ModalDialogProperties.ButtonType.NEGATIVE) {
                        mModalDialogManager.dismissDialog(
                                mModalDialogPropertyModel,
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
     * @param quickDeleteView {@link View} of the quick delete.
     * @param modalDialogManager A {@link ModalDialogManager} responsible for showing the quick
     *     delete modal dialog.
     * @param onDismissCallback A {@link Callback} that will be notified when the user confirms or
     *     cancels the deletion;
     * @param tabModelSelector {@link TabModelSelector} to use for opening the links in search
     *     history disambiguation notice.
     * @param timePeriodChangeObserver {@link TimePeriodChangeObserver} which would be notified when
     *     the spinner is toggled.
     */
    QuickDeleteDialogDelegate(
            @NonNull Context context,
            @NonNull View quickDeleteView,
            @NonNull ModalDialogManager modalDialogManager,
            @NonNull Callback<Integer> onDismissCallback,
            @NonNull TabModelSelector tabModelSelector,
            @NonNull TimePeriodChangeObserver timePeriodChangeObserver) {
        mContext = context;
        mQuickDeleteView = quickDeleteView;
        mModalDialogManager = modalDialogManager;
        mOnDismissCallback = onDismissCallback;
        mTabModelSelector = tabModelSelector;
        mTimePeriodChangeObserver = timePeriodChangeObserver;

        mCurrentTimePeriodOption =
                new TimePeriodSpinnerOption(
                        TimePeriod.LAST_15_MINUTES,
                        mContext.getString(R.string.clear_browsing_data_tab_period_15_minutes));
    }

    /** A method to create the dialog attributes for the quick delete dialog. */
    private PropertyModel createQuickDeleteDialogProperty() {
        // Update Spinner
        Spinner quickDeleteSpinner = mQuickDeleteView.findViewById(R.id.quick_delete_spinner);
        updateSpinner(quickDeleteSpinner);

        // Update the "More options" button.
        ButtonCompat moreOptionsView =
                mQuickDeleteView.findViewById(R.id.quick_delete_more_options);
        moreOptionsView.setOnClickListener(view -> openClearBrowsingDataDialog());

        // Update search history text
        setUpSearchHistoryText();

        return new PropertyModel.Builder(ModalDialogProperties.ALL_KEYS)
                .with(ModalDialogProperties.CONTROLLER, mModalDialogController)
                .with(ModalDialogProperties.CUSTOM_VIEW, mQuickDeleteView)
                .with(
                        ModalDialogProperties.POSITIVE_BUTTON_TEXT,
                        mContext.getString(R.string.clear_data_delete))
                .with(
                        ModalDialogProperties.NEGATIVE_BUTTON_TEXT,
                        mContext.getString(R.string.cancel))
                .with(ModalDialogProperties.CANCEL_ON_TOUCH_OUTSIDE, true)
                .with(
                        ModalDialogProperties.BUTTON_STYLES,
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
        ArrayAdapter<TimePeriodSpinnerOption> adapter =
                new ArrayAdapter<>(
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
        quickDeleteSpinner.setOnItemSelectedListener(
                new AdapterView.OnItemSelectedListener() {
                    @Override
                    public void onItemSelected(
                            AdapterView<?> adapterView, View view, int position, long id) {
                        TimePeriodSpinnerOption item =
                                (TimePeriodSpinnerOption) adapterView.getItemAtPosition(position);
                        mCurrentTimePeriodOption = item;
                        @TimePeriod int timePeriod = mCurrentTimePeriodOption.getTimePeriod();
                        mTimePeriodChangeObserver.onTimePeriodChanged(timePeriod);
                        recordTimePeriodChange(timePeriod);
                    }

                    @Override
                    public void onNothingSelected(AdapterView<?> adapterView) {
                        // Revert back to default time.
                        String message =
                                mContext.getString(
                                        R.string.clear_browsing_data_tab_period_15_minutes);
                        mCurrentTimePeriodOption =
                                new TimePeriodSpinnerOption(TimePeriod.LAST_15_MINUTES, message);
                    }
                });
    }

    private void openClearBrowsingDataDialog() {
        QuickDeleteMetricsDelegate.recordHistogram(
                QuickDeleteMetricsDelegate.QuickDeleteAction.MORE_OPTIONS_CLICKED);
        SettingsNavigationFactory.createSettingsNavigation()
                .startSettings(
                        mContext,
                        SettingsNavigation.SettingsFragment.CLEAR_BROWSING_DATA_ADVANCED_PAGE);
        mModalDialogManager.dismissDialog(
                mModalDialogPropertyModel, DialogDismissalCause.ACTION_ON_CONTENT);
    }

    private void setUpSearchHistoryText() {
        TextViewWithClickableSpans text =
                mQuickDeleteView.findViewById(R.id.search_history_disambiguation);
        Callback<View> openHistoryCallback =
                (widget) -> openUrlInNewTab(UrlConstants.GOOGLE_SEARCH_HISTORY_URL_IN_QD);
        Callback<View> openActivityCallback =
                (widget) -> openUrlInNewTab(UrlConstants.MY_ACTIVITY_URL_IN_QD);
        final SpannableString searchHistoryText =
                SpanApplier.applySpans(
                        mContext.getString(
                                R.string.quick_delete_dialog_search_history_disambiguation_text),
                        new SpanApplier.SpanInfo(
                                "<link1>",
                                "</link1>",
                                new NoUnderlineClickableSpan(mContext, openHistoryCallback)),
                        new SpanApplier.SpanInfo(
                                "<link2>",
                                "</link2>",
                                new NoUnderlineClickableSpan(mContext, openActivityCallback)));
        text.setText(searchHistoryText);
        text.setMovementMethod(LinkMovementMethod.getInstance());
    }

    /**
     * Opens a url in a new non-incognito tab and dismisses the dialog.
     *
     * @param url The URL of the page to load, either GOOGLE_SEARCH_HISTORY_URL_IN_QD or
     *            MY_ACTIVITY_URL_IN_QD.
     */
    private void openUrlInNewTab(final String url) {
        if (Objects.equals(url, UrlConstants.GOOGLE_SEARCH_HISTORY_URL_IN_QD)) {
            QuickDeleteMetricsDelegate.recordHistogram(
                    QuickDeleteMetricsDelegate.QuickDeleteAction.SEARCH_HISTORY_LINK_CLICKED);
        } else {
            QuickDeleteMetricsDelegate.recordHistogram(
                    QuickDeleteMetricsDelegate.QuickDeleteAction.MY_ACTIVITY_LINK_CLICKED);
        }

        mTabModelSelector.openNewTab(
                new LoadUrlParams(url),
                TabLaunchType.FROM_LINK,
                mTabModelSelector.getCurrentTab(),
                false);
        mModalDialogManager.dismissDialog(
                mModalDialogPropertyModel, DialogDismissalCause.ACTION_ON_CONTENT);
    }

    private void recordTimePeriodChange(@TimePeriod int timePeriod) {
        switch (timePeriod) {
            case TimePeriod.LAST_15_MINUTES:
                QuickDeleteMetricsDelegate.recordHistogram(
                        QuickDeleteMetricsDelegate.QuickDeleteAction.LAST_15_MINUTES_SELECTED);
                break;
            case TimePeriod.LAST_HOUR:
                QuickDeleteMetricsDelegate.recordHistogram(
                        QuickDeleteMetricsDelegate.QuickDeleteAction.LAST_HOUR_SELECTED);
                break;
            case TimePeriod.LAST_DAY:
                QuickDeleteMetricsDelegate.recordHistogram(
                        QuickDeleteMetricsDelegate.QuickDeleteAction.LAST_DAY_SELECTED);
                break;
            case TimePeriod.LAST_WEEK:
                QuickDeleteMetricsDelegate.recordHistogram(
                        QuickDeleteMetricsDelegate.QuickDeleteAction.LAST_WEEK_SELECTED);
                break;
            case TimePeriod.FOUR_WEEKS:
                QuickDeleteMetricsDelegate.recordHistogram(
                        QuickDeleteMetricsDelegate.QuickDeleteAction.FOUR_WEEKS_SELECTED);
                break;
            case TimePeriod.ALL_TIME:
                QuickDeleteMetricsDelegate.recordHistogram(
                        QuickDeleteMetricsDelegate.QuickDeleteAction.ALL_TIME_SELECTED);
                break;
            default:
                throw new IllegalStateException("Unexpected value: " + timePeriod);
        }
    }

    /** Shows the Quick delete dialog. */
    void showDialog() {
        mModalDialogPropertyModel = createQuickDeleteDialogProperty();
        mModalDialogManager.showDialog(
                mModalDialogPropertyModel, ModalDialogManager.ModalDialogType.APP);
    }
}
