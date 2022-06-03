// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.datareduction;

import android.content.Context;

import org.chromium.base.CommandLine;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.net.spdyproxy.DataReductionProxySettings;
import org.chromium.components.browser_ui.util.ConversionUtils;
import org.chromium.components.variations.VariationsAssociatedData;

/**
 * The controller logic for the Data Reduction Proxy savings milestone promo that lets users of the
 * proxy know when Chrome has saved a given amount of data. Each decision to show the promo should
 * be made by a separate instance of this class, using the provided getter methods.
 */
public class DataReductionSavingsMilestonePromo {
    /**
     * A semi-colon delimited list of data savings values in MB that the promo should be shown
     * for.
     */
    public static final String PROMO_PARAM_NAME = "x_milestone_promo_data_savings_in_megabytes";

    public static final String PROMO_FIELD_TRIAL_NAME = "DataCompressionProxyPromoVisibility";
    private static final String ENABLE_DATA_REDUCTION_PROXY_SAVINGS_PROMO_SWITCH =
            "enable-data-reduction-proxy-savings-promo";

    private final Context mContext;

    /** The current data savings to promote. */
    private final long mDataSavingsInBytes;

    /** If not null, this promo should be shown to the user. */
    private String mPromoText;

    /**
     * Constructs a fully initialized DataReductionSavingsMilestonePromo instance.
     *
     * This' getters are ready immediately after construction. If shouldShowPromo is true and the
     * promo text is shown to the user, onPromoTextSeen should be called on the same instance.
     *
     * @param context The context.
     * @param dataSavingsInBytes The amount of data the Data Reduction Proxy has saved in bytes.
     */
    public DataReductionSavingsMilestonePromo(Context context, long dataSavingsInBytes) {
        mContext = context;
        mDataSavingsInBytes = dataSavingsInBytes;
        mPromoText = computeDataReductionMilestonePromo();
    }

    /**
     * Returns true if the promo can be shown to the user. If shown, onPromoTextSeen should be
     * called.
     */
    public boolean shouldShowPromo() {
        return mPromoText != null;
    }

    /**
     * Returns the promo text to be shown to the user. Once shown, onPromoTextSeen should be called.
     */
    public String getPromoText() {
        return mPromoText;
    }

    /**
     * This should be called after the promo text is shown to the user.
     */
    public void onPromoTextSeen() {
        assert (shouldShowPromo());
        DataReductionPromoUtils.saveMilestonePromoDisplayed(mDataSavingsInBytes);
    }

    /**
     * Decides whether the Data Reduction Proxy promo should be shown given the current data savings
     * and specific thresholds set by variations.
     *
     * @return The string to use in the promo. If null or empty, do not show a promo.
     */
    private String computeDataReductionMilestonePromo() {
        // Skip if Lite mode is not enabled.
        if (!DataReductionProxySettings.getInstance().isDataReductionProxyEnabled()) {
            return null;
        }
        // Prevents users who upgrade and have already saved more than the threshold from seeing the
        // promo.
        if (!DataReductionPromoUtils.hasMilestonePromoBeenInitWithStartingSavedBytes()) {
            DataReductionPromoUtils.saveMilestonePromoDisplayed(mDataSavingsInBytes);
            return null;
        }

        String variationParamValue = VariationsAssociatedData.getVariationParamValue(
                PROMO_FIELD_TRIAL_NAME, PROMO_PARAM_NAME);
        int[] promoDataSavingsMB;

        if (variationParamValue.isEmpty()) {
            if (CommandLine.getInstance().hasSwitch(
                        ENABLE_DATA_REDUCTION_PROXY_SAVINGS_PROMO_SWITCH)) {
                promoDataSavingsMB = new int[1];
                promoDataSavingsMB[0] = 1;
            } else {
                promoDataSavingsMB = new int[0];
            }
        } else {
            variationParamValue = variationParamValue.replace(" ", "");
            String[] promoDataSavingStrings = variationParamValue.split(";");

            if (CommandLine.getInstance().hasSwitch(
                        ENABLE_DATA_REDUCTION_PROXY_SAVINGS_PROMO_SWITCH)) {
                promoDataSavingsMB = new int[promoDataSavingStrings.length + 1];
                promoDataSavingsMB[promoDataSavingStrings.length] = 1;
            } else {
                promoDataSavingsMB = new int[promoDataSavingStrings.length];
            }

            for (int i = 0; i < promoDataSavingStrings.length; i++) {
                try {
                    promoDataSavingsMB[i] = Integer.parseInt(promoDataSavingStrings[i]);
                } catch (NumberFormatException e) {
                    promoDataSavingsMB[i] = -1;
                }
            }
        }

        for (int threshold : promoDataSavingsMB) {
            long promoDataSavingsBytes = threshold * ConversionUtils.BYTES_PER_MEGABYTE;
            if (threshold > 0 && mDataSavingsInBytes >= promoDataSavingsBytes
                    && DataReductionPromoUtils.getDisplayedMilestonePromoSavedBytes()
                            < promoDataSavingsBytes) {
                return getStringForBytes(mContext, promoDataSavingsBytes);
            }
        }
        return null;
    }

    private static String getStringForBytes(Context context, long bytes) {
        int resourceId;
        int bytesInCorrectUnits;

        if (bytes < ConversionUtils.BYTES_PER_GIGABYTE) {
            resourceId = R.string.data_reduction_milestone_promo_text_mb;
            bytesInCorrectUnits = (int) ConversionUtils.bytesToMegabytes(bytes);
        } else {
            resourceId = R.string.data_reduction_milestone_promo_text_gb;
            bytesInCorrectUnits = (int) ConversionUtils.bytesToGigabytes(bytes);
        }

        return context.getResources().getString(resourceId, bytesInCorrectUnits);
    }
}
