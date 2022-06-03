// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.datareduction.settings;

import android.content.Context;
import android.graphics.drawable.Drawable;
import android.text.format.Formatter;
import android.util.AttributeSet;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.LinearLayout;
import android.widget.TableLayout;
import android.widget.TableRow;
import android.widget.TextView;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.datareduction.DataReductionProxyUma;

import java.io.Serializable;
import java.util.Collections;
import java.util.Comparator;
import java.util.List;

/**
 * A site breakdown view to be used by the Data Saver settings page. It displays the top ten sites
 * with the most data use or data savings.
 */
public class DataReductionSiteBreakdownView extends LinearLayout {
    private static final int NUM_DATA_USE_ITEMS_TO_ADD = 10;

    /**
     * Hostname used for the other bucket which consists of chrome-services traffic.
     * This should be in sync with the same in DataReductionProxyDataUseObserver.
     */
    private static final String OTHER_HOST_NAME = "Other";

    private int mNumDataUseItemsToDisplay = 10;

    private TableLayout mTableLayout;
    private TextView mDetailsTitle;
    private TextView mHostnameTitle;
    private TextView mDataUsedTitle;
    private TextView mDataSavedTitle;
    private List<DataReductionDataUseItem> mDataUseItems;
    private boolean mTextViewsNeedAttributesSet;

    public DataReductionSiteBreakdownView(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();
        mTableLayout = (TableLayout) findViewById(R.id.data_reduction_proxy_breakdown_table);
        mDetailsTitle = (TextView) findViewById(R.id.data_reduction_data_usage_breakdown_title);
        mHostnameTitle = (TextView) findViewById(R.id.data_reduction_breakdown_site_title);
        mDataUsedTitle = (TextView) findViewById(R.id.data_reduction_breakdown_used_title);
        mDataSavedTitle = (TextView) findViewById(R.id.data_reduction_breakdown_saved_title);

        mHostnameTitle.setOnClickListener(new OnClickListener() {
            @Override
            public void onClick(View v) {
                DataReductionProxyUma.dataReductionProxyUIAction(
                        DataReductionProxyUma.ACTION_SITE_BREAKDOWN_SORTED_BY_HOSTNAME);
                setTextViewUnsortedAttributes(mDataSavedTitle);
                setTextViewUnsortedAttributes(mDataUsedTitle);
                setTextViewSortedAttributes(mHostnameTitle);
                Collections.sort(mDataUseItems, new HostnameComparator());
                mDetailsTitle.setContentDescription(
                        getContext().getString(R.string.data_reduction_breakdown_hostname_sorted));
                updateSiteBreakdown();
            }
        });

        mDataUsedTitle.setOnClickListener(new OnClickListener() {
            @Override
            public void onClick(View v) {
                DataReductionProxyUma.dataReductionProxyUIAction(
                        DataReductionProxyUma.ACTION_SITE_BREAKDOWN_SORTED_BY_DATA_USED);
                setTextViewUnsortedAttributes(mHostnameTitle);
                setTextViewUnsortedAttributes(mDataSavedTitle);
                setTextViewSortedAttributes(mDataUsedTitle);
                Collections.sort(mDataUseItems, new DataUsedComparator());
                mDetailsTitle.setContentDescription(
                        getContext().getString(R.string.data_reduction_breakdown_data_used_sorted));
                updateSiteBreakdown();
            }
        });

        mDataSavedTitle.setOnClickListener(new OnClickListener() {
            @Override
            public void onClick(View v) {
                DataReductionProxyUma.dataReductionProxyUIAction(
                        DataReductionProxyUma.ACTION_SITE_BREAKDOWN_SORTED_BY_DATA_SAVED);
                setTextViewUnsortedAttributes(mHostnameTitle);
                setTextViewUnsortedAttributes(mDataUsedTitle);
                setTextViewSortedAttributes(mDataSavedTitle);
                Collections.sort(mDataUseItems, new DataSavedComparator());
                mDetailsTitle.setContentDescription(getContext().getString(
                        R.string.data_reduction_breakdown_data_saved_sorted));
                updateSiteBreakdown();
            }
        });
    }

    @Override
    protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
        super.onMeasure(widthMeasureSpec, heightMeasureSpec);
        if (mTextViewsNeedAttributesSet) {
            mTextViewsNeedAttributesSet = false;
            setTextViewUnsortedAttributes(mHostnameTitle);
            setTextViewUnsortedAttributes(mDataUsedTitle);
            setTextViewSortedAttributes(mDataSavedTitle);
        }
    }

    /**
     * Set and display the data use items once they have been fetched from the compression stats.
     * @param items A list of items split by hostname to show in the breakdown.
     */
    public void setAndDisplayDataUseItems(List<DataReductionDataUseItem> items) {
        mDataUseItems = items;
        setTextViewUnsortedAttributes(mHostnameTitle);
        setTextViewUnsortedAttributes(mDataUsedTitle);
        setTextViewSortedAttributes(mDataSavedTitle);
        Collections.sort(mDataUseItems, new DataSavedComparator());
        mDetailsTitle.setContentDescription(
                getContext().getString(R.string.data_reduction_breakdown_data_saved_sorted));
        if (mDataUseItems.size() == 0) {
            setVisibility(GONE);
        } else {
            setVisibility(VISIBLE);
            updateSiteBreakdown();
            DataReductionProxyUma.dataReductionProxyUIAction(
                    DataReductionProxyUma.ACTION_SITE_BREAKDOWN_DISPLAYED);
        }
    }

    private void setTextViewSortedAttributes(TextView textView) {
        Drawable arrowDrawable = getStartCompoundDrawable(textView);
        // If the drawable has not been created yet, set mTextViewsNeedAttributesSet so that
        // onMeasure will set the attributes after the drawable is created.
        if (arrowDrawable == null) {
            mTextViewsNeedAttributesSet = true;
            return;
        }
        // Expose the arrow for this sorted column using its text color.
        arrowDrawable.mutate();
        arrowDrawable.setAlpha(255);
        arrowDrawable.setColorFilter(new android.graphics.PorterDuffColorFilter(
                textView.getCurrentTextColor(), android.graphics.PorterDuff.Mode.SRC_IN));
    }

    private void setTextViewUnsortedAttributes(TextView textView) {
        Drawable arrowDrawable = getStartCompoundDrawable(textView);
        // If the drawable has not been created yet, set mTextViewsNeedAttributesSet so that
        // onMeasure will set the attributes after the drawable is created.
        if (arrowDrawable == null) {
            mTextViewsNeedAttributesSet = true;
            return;
        }
        // Clear the arrow from this unsorted column.
        arrowDrawable.mutate();
        arrowDrawable.setAlpha(0);
        arrowDrawable.clearColorFilter();
    }

    private Drawable getStartCompoundDrawable(TextView textView) {
        Drawable[] drawables = textView.getCompoundDrawables();
        // Start drawable can be in the left or right index based on if the layout is rtl.
        if (drawables[0] != null) {
            return drawables[0];
        }
        return drawables[2];
    }

    /**
     * Sorts the DataReductionDataUseItems by hostname.
     */
    private static final class HostnameComparator
            implements Comparator<DataReductionDataUseItem>, Serializable {
        @Override
        public int compare(DataReductionDataUseItem lhs, DataReductionDataUseItem rhs) {
            // Force the 'Other' category to the bottom of the list.
            if (OTHER_HOST_NAME.equals(lhs.getHostname())) {
                return 1;
            } else if (OTHER_HOST_NAME.equals(rhs.getHostname())) {
                return -1;
            }
            return lhs.getHostname().compareTo(rhs.getHostname());
        }
    }

    /**
     * Sorts the DataReductionDataUseItems by most to least data used.
     */
    private static final class DataUsedComparator
            implements Comparator<DataReductionDataUseItem>, Serializable {
        @Override
        public int compare(DataReductionDataUseItem lhs, DataReductionDataUseItem rhs) {
            // Force the 'Other' category to the bottom of the list.
            if (OTHER_HOST_NAME.equals(lhs.getHostname())) {
                return 1;
            } else if (OTHER_HOST_NAME.equals(rhs.getHostname())) {
                return -1;
            } else if (lhs.getDataUsed() < rhs.getDataUsed()) {
                return 1;
            } else if (lhs.getDataUsed() > rhs.getDataUsed()) {
                return -1;
            }
            return 0;
        }
    }

    /**
     * Sorts the DataReductionDataUseItems by most to least data saved. If data saved is equal, most
     * likely because both items have zero data saving, then sort by data used.
     */
    private static class DataSavedComparator
            implements Comparator<DataReductionDataUseItem>, Serializable {
        @Override
        public int compare(DataReductionDataUseItem lhs, DataReductionDataUseItem rhs) {
            // Force the 'Other' category to the bottom of the list.
            if (OTHER_HOST_NAME.equals(lhs.getHostname())) {
                return 1;
            } else if (OTHER_HOST_NAME.equals(rhs.getHostname())) {
                return -1;
            } else if (lhs.getDataSaved() < rhs.getDataSaved()) {
                return 1;
            } else if (lhs.getDataSaved() > rhs.getDataSaved()) {
                return -1;
            } else if (lhs.getDataUsed() < rhs.getDataUsed()) {
                return 1;
            } else if (lhs.getDataUsed() > rhs.getDataUsed()) {
                return -1;
            }
            return 0;
        }
    }

    /**
     * Update the site breakdown to display the data use items.
     */
    private void updateSiteBreakdown() {
        // Remove all old rows except the header.
        mTableLayout.removeViews(1, mTableLayout.getChildCount() - 1);

        int numRemainingSites = 0;
        long everythingElseDataUsage = 0;
        long everythingElseDataSavings = 0;

        for (int i = 0; i < mDataUseItems.size(); i++) {
            if (i < mNumDataUseItemsToDisplay) {
                TableRow row = (TableRow) LayoutInflater.from(getContext())
                                       .inflate(R.layout.data_usage_breakdown_row, null);

                TextView hostnameView = (TextView) row.findViewById(R.id.site_hostname);
                TextView dataUsedView = (TextView) row.findViewById(R.id.site_data_used);
                TextView dataSavedView = (TextView) row.findViewById(R.id.site_data_saved);

                String hostName = mDataUseItems.get(i).getHostname();
                if (OTHER_HOST_NAME.equals(hostName)) {
                    hostName = getResources().getString(
                            R.string.data_reduction_breakdown_other_host_name);
                }
                hostnameView.setText(hostName);

                final CharSequence dataUsed =
                        mDataUseItems.get(i).getFormattedDataUsed(getContext());
                dataUsedView.setText(dataUsed);
                dataUsedView.setContentDescription(getResources().getString(
                        R.string.data_reduction_breakdown_used_content_description, dataUsed));

                final CharSequence dataSaved =
                        mDataUseItems.get(i).getFormattedDataSaved(getContext());
                dataSavedView.setText(dataSaved);
                dataSavedView.setContentDescription(getResources().getString(
                        R.string.data_reduction_breakdown_saved_content_description, dataSaved));

                mTableLayout.addView(row, i + 1);
            } else {
                numRemainingSites++;
                everythingElseDataUsage += mDataUseItems.get(i).getDataUsed();
                everythingElseDataSavings += mDataUseItems.get(i).getDataSaved();
            }
        }

        if (numRemainingSites > 0) {
            TableRow row = (TableRow) LayoutInflater.from(getContext())
                                   .inflate(R.layout.data_usage_breakdown_row, null);

            TextView hostnameView = (TextView) row.findViewById(R.id.site_hostname);
            TextView dataUsedView = (TextView) row.findViewById(R.id.site_data_used);
            TextView dataSavedView = (TextView) row.findViewById(R.id.site_data_saved);

            hostnameView.setText(getResources().getString(
                    R.string.data_reduction_breakdown_remaining_sites_label, numRemainingSites));
            dataUsedView.setText(Formatter.formatFileSize(getContext(), everythingElseDataUsage));
            dataSavedView.setText(
                    Formatter.formatFileSize(getContext(), everythingElseDataSavings));

            int textColorLink = ApiCompatibilityUtils.getColor(
                    getContext().getResources(), R.color.default_text_color_link);

            hostnameView.setTextColor(textColorLink);
            dataUsedView.setTextColor(textColorLink);
            dataSavedView.setTextColor(textColorLink);

            row.setOnClickListener(new OnClickListener() {
                @Override
                public void onClick(View v) {
                    DataReductionProxyUma.dataReductionProxyUIAction(
                            DataReductionProxyUma.ACTION_SITE_BREAKDOWN_EXPANDED);
                    mNumDataUseItemsToDisplay += NUM_DATA_USE_ITEMS_TO_ADD;
                    updateSiteBreakdown();
                }
            });

            mTableLayout.addView(row, mNumDataUseItemsToDisplay + 1);
        }

        mTableLayout.requestLayout();
    }
}
