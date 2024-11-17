// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.quick_delete;

import static android.view.View.GONE;
import static android.view.View.VISIBLE;

import android.content.Context;
import android.graphics.PorterDuff;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ImageView;
import android.widget.TextView;

import androidx.annotation.ColorInt;
import androidx.annotation.NonNull;
import androidx.annotation.VisibleForTesting;
import androidx.core.content.ContextCompat;

import org.chromium.chrome.browser.browsing_data.TimePeriod;
import org.chromium.chrome.browser.quick_delete.QuickDeleteDelegate.DomainVisitsData;
import org.chromium.components.browser_ui.widget.text.TemplatePreservingTextView;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.widget.TextViewWithClickableSpans;

/** The {@View} binder class for the quick delete MVC. */
class QuickDeleteViewBinder {
    public static void bind(PropertyModel model, View quickDeleteView, PropertyKey propertyKey) {
        if (QuickDeleteProperties.DOMAIN_VISITED_DATA == propertyKey) {
            assert model.get(QuickDeleteProperties.DOMAIN_VISITED_DATA) != null;
            updateBrowsingHistoryRow(
                    model.get(QuickDeleteProperties.CONTEXT),
                    quickDeleteView,
                    model.get(QuickDeleteProperties.DOMAIN_VISITED_DATA),
                    model.get(QuickDeleteProperties.TIME_PERIOD));
        } else if (QuickDeleteProperties.CLOSED_TABS_COUNT == propertyKey
                || QuickDeleteProperties.TIME_PERIOD == propertyKey) {
            if (model.get(QuickDeleteProperties.HAS_MULTI_WINDOWS)) {
                disableTabsRow(model.get(QuickDeleteProperties.CONTEXT), quickDeleteView);
            } else {
                updateClosedTabsCount(
                        model.get(QuickDeleteProperties.CONTEXT),
                        quickDeleteView,
                        model.get(QuickDeleteProperties.CLOSED_TABS_COUNT),
                        model.get(QuickDeleteProperties.TIME_PERIOD));
            }
        } else if (QuickDeleteProperties.IS_SIGNED_IN == propertyKey) {
            updateSearchHistoryVisibility(
                    quickDeleteView, model.get(QuickDeleteProperties.IS_SIGNED_IN));
        } else if (QuickDeleteProperties.IS_SYNCING_HISTORY == propertyKey) {
            updateBrowsingHistorySubtitleVisibility(
                    quickDeleteView, model.get(QuickDeleteProperties.IS_SYNCING_HISTORY));
        } else if (QuickDeleteProperties.IS_DOMAIN_VISITED_DATA_PENDING == propertyKey) {
            updateBrowsingHistoryRowIfPending(
                    model.get(QuickDeleteProperties.CONTEXT),
                    quickDeleteView,
                    model.get(QuickDeleteProperties.IS_DOMAIN_VISITED_DATA_PENDING));
        }
    }

    private static void disableTabsRow(@NonNull Context context, @NonNull View quickDeleteView) {
        ViewGroup tabsRow = quickDeleteView.findViewById(R.id.quick_delete_tabs_close_row);
        TextView title = tabsRow.findViewById(R.id.quick_delete_tabs_row_title);
        TextView subtitle = tabsRow.findViewById(R.id.quick_delete_tabs_row_subtitle);
        ImageView icon = tabsRow.findViewById(R.id.quick_delete_tabs_row_icon);

        title.setEnabled(false);
        title.setText(context.getString(R.string.quick_delete_tabs_title));

        subtitle.setEnabled(false);
        subtitle.setText(context.getString(R.string.clear_tabs_disabled_summary));
        subtitle.setVisibility(VISIBLE);

        final @ColorInt int color =
                ContextCompat.getColor(tabsRow.getContext(), R.color.default_icon_color_disabled);

        icon.setColorFilter(color, PorterDuff.Mode.SRC_IN);
    }

    private static void updateSearchHistoryVisibility(
            @NonNull View quickDeleteView, boolean isVisible) {
        TextViewWithClickableSpans searchHistoryTextView =
                quickDeleteView.findViewById(R.id.search_history_disambiguation);
        searchHistoryTextView.setVisibility(isVisible ? VISIBLE : GONE);
    }

    private static void updateBrowsingHistorySubtitleVisibility(
            @NonNull View quickDeleteView, boolean isVisible) {
        ViewGroup quickDeleteHistoryRow =
                quickDeleteView.findViewById(R.id.quick_delete_history_row);
        TextView subtitle =
                quickDeleteHistoryRow.findViewById(R.id.quick_delete_history_row_subtitle);
        subtitle.setVisibility(isVisible ? VISIBLE : GONE);
    }

    private static void updateBrowsingHistoryRowIfPending(
            @NonNull Context context, @NonNull View quickDeleteView, boolean isPending) {
        if (!isPending) return;
        ViewGroup quickDeleteHistoryRow =
                quickDeleteView.findViewById(R.id.quick_delete_history_row);
        TemplatePreservingTextView title =
                quickDeleteHistoryRow.findViewById(R.id.quick_delete_history_row_title);
        title.setTemplate(null);
        title.setText(context.getString(R.string.quick_delete_dialog_data_pending));
        quickDeleteHistoryRow.setVisibility(VISIBLE);
    }

    private static void updateBrowsingHistoryRow(
            @NonNull Context context,
            @NonNull View quickDeleteView,
            @NonNull DomainVisitsData domainVisitsData,
            @TimePeriod int timePeriod) {
        String browsingHistoryRowTitleTemplate = null;
        int domainsCount = domainVisitsData.mDomainsCount;
        ViewGroup historyRow = quickDeleteView.findViewById(R.id.quick_delete_history_row);
        TemplatePreservingTextView title =
                historyRow.findViewById(R.id.quick_delete_history_row_title);

        if (domainsCount == 0) {
            title.setTemplate(browsingHistoryRowTitleTemplate);

            if (timePeriod == TimePeriod.ALL_TIME) {
                title.setText(
                        context.getString(
                                R.string
                                        .quick_delete_dialog_zero_browsing_history_domain_count_all_time_text));
            } else {
                title.setText(
                        context.getString(
                                R.string
                                        .quick_delete_dialog_zero_browsing_history_domain_count_text,
                                getTimePeriodString(context, timePeriod)));
            }
            return;
        }

        // Subtract 1 from the domainsCount to not count the lastVisitedDomain twice.
        domainsCount--;

        // If there is at least 1 other site counted, add the count template, eg `+ 1 site`.
        if (domainsCount > 0) {
            String domainCountText =
                    context.getResources()
                            .getQuantityString(
                                    R.plurals
                                            .quick_delete_dialog_browsing_history_domain_count_text,
                                    domainsCount,
                                    domainsCount);
            browsingHistoryRowTitleTemplate = "%s " + domainCountText;
        }

        title.setTemplate(browsingHistoryRowTitleTemplate);
        title.setText(domainVisitsData.mLastVisitedDomain);
    }

    private static void updateClosedTabsCount(
            @NonNull Context context,
            @NonNull View quickDeleteView,
            int tabsCount,
            @TimePeriod int timePeriod) {
        ViewGroup tabsRow = quickDeleteView.findViewById(R.id.quick_delete_tabs_close_row);
        TextView title = tabsRow.findViewById(R.id.quick_delete_tabs_row_title);

        if (tabsCount > 0) {
            String tabDescription =
                    context.getResources()
                            .getQuantityString(
                                    R.plurals.quick_delete_dialog_tabs_closed_text,
                                    tabsCount,
                                    tabsCount);
            title.setText(tabDescription);
        } else {
            if (timePeriod == TimePeriod.ALL_TIME) {
                title.setText(
                        context.getString(
                                R.string.quick_delete_dialog_zero_tabs_closed_all_time_text));
            } else {
                title.setText(
                        context.getString(
                                R.string.quick_delete_dialog_zero_tabs_closed_text,
                                getTimePeriodString(context, timePeriod)));
            }
        }
    }

    @VisibleForTesting
    static String getTimePeriodString(Context context, @TimePeriod int timePeriod) {
        switch (timePeriod) {
            case TimePeriod.LAST_15_MINUTES:
                return context.getString(R.string.quick_delete_time_period_15_minutes);
            case TimePeriod.LAST_HOUR:
                return context.getString(R.string.quick_delete_time_period_hour);
            case TimePeriod.LAST_DAY:
                return context.getString(R.string.quick_delete_time_period_24_hours);
            case TimePeriod.LAST_WEEK:
                return context.getString(R.string.quick_delete_time_period_7_days);
            case TimePeriod.FOUR_WEEKS:
                return context.getString(R.string.quick_delete_time_period_four_weeks);
            default:
                throw new IllegalStateException("Unexpected value: " + timePeriod);
        }
    }
}
