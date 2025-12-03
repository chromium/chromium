// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.notifications.tips;

import android.view.View.OnClickListener;

import androidx.annotation.IntDef;

import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModel.WritableIntPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.List;

/** Properties for the Tips Notifications promo bottom sheet. */
@NullMarked
public class TipsPromoProperties {
    /** The different screens that can be shown on the sheet. */
    @IntDef({
        ScreenType.MAIN_SCREEN,
        ScreenType.DETAIL_SCREEN,
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface ScreenType {
        int MAIN_SCREEN = 0;
        int DETAIL_SCREEN = 1;
    }

    /** Contains information needed by the Tips Promo View to display UI. */
    @NullMarked
    public static class FeatureTipPromoData {
        public final String positiveButtonText;
        public final String mainPageTitle;
        public final String mainPageDescription;
        public final int mainPageLogoViewRes;
        public final String detailPageTitle;
        public final List<String> detailPageSteps;

        /**
         * Create a {@link FeatureTipPromoData} object containing feature tip data.
         *
         * @param positiveButtonText The text on the positive button on the promo.
         * @param mainPageTitle The title of the main page of the promo.
         * @param mainPageDescription The description of the main page of the promo.
         * @param mainPageLogoViewRes The resource id of the main page logo view.
         * @param detailPageTitle The title of the detail page of the promo.
         * @param detailPageSteps The info steps for the detail page of the promo.
         */
        public FeatureTipPromoData(
                String positiveButtonText,
                String mainPageTitle,
                String mainPageDescription,
                int mainPageLogoViewRes,
                String detailPageTitle,
                List<String> detailPageSteps) {
            this.positiveButtonText = positiveButtonText;
            this.mainPageTitle = mainPageTitle;
            this.mainPageDescription = mainPageDescription;
            this.mainPageLogoViewRes = mainPageLogoViewRes;
            this.detailPageTitle = detailPageTitle;
            this.detailPageSteps = detailPageSteps;
        }
    }

    /** The {@link FeatureTipPromoData} containing information for each tip. */
    public static final WritableObjectPropertyKey<FeatureTipPromoData> FEATURE_TIP_PROMO_DATA =
            new WritableObjectPropertyKey<>();

    /** Indicates which {@link ScreenType} is currently displayed on the bottom sheet. */
    public static final WritableIntPropertyKey CURRENT_SCREEN = new WritableIntPropertyKey();

    /** Click listener for the back button. */
    public static final WritableObjectPropertyKey<OnClickListener> BACK_BUTTON_CLICK_LISTENER =
            new WritableObjectPropertyKey<>();

    /** Click listener for the details button. */
    public static final WritableObjectPropertyKey<OnClickListener> DETAILS_BUTTON_CLICK_LISTENER =
            new WritableObjectPropertyKey<>();

    /** Click listener for the settings button. */
    public static final WritableObjectPropertyKey<OnClickListener> SETTINGS_BUTTON_CLICK_LISTENER =
            new WritableObjectPropertyKey<>();

    public static final PropertyKey[] ALL_KEYS = {
        FEATURE_TIP_PROMO_DATA,
        CURRENT_SCREEN,
        BACK_BUTTON_CLICK_LISTENER,
        DETAILS_BUTTON_CLICK_LISTENER,
        SETTINGS_BUTTON_CLICK_LISTENER
    };

    /**
     * Creates a default model structure.
     *
     * @return A new {@link PropertyModel} instance.
     */
    public static PropertyModel createDefaultModel() {
        return new PropertyModel.Builder(ALL_KEYS).build();
    }
}
