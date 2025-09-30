// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.notifications.tips;

import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;

/** Properties for the Tips Notifications promo bottom sheet. */
@NullMarked
public class TipsPromoProperties {
    /** Contains information needed by the Tips Promo View to display UI. */
    public static class FeatureTipPromoData {
        public final String mainPageTitle;
        public final String mainPageDescription;

        /**
         * Create a {@link FeatureTipPromoData} object containing feature tip data.
         *
         * @param mainPageTitle The title of the main page of the promo.
         * @param mainPageDescription The description of the main page of the promo.
         */
        public FeatureTipPromoData(String mainPageTitle, String mainPageDescription) {
            this.mainPageTitle = mainPageTitle;
            this.mainPageDescription = mainPageDescription;
        }
    }

    public static final WritableObjectPropertyKey<FeatureTipPromoData> FEATURE_TIP_PROMO_DATA =
            new WritableObjectPropertyKey<>();

    public static final PropertyKey[] ALL_KEYS = {FEATURE_TIP_PROMO_DATA};

    /**
     * Creates a default model structure.
     *
     * @return A new {@link PropertyModel} instance.
     */
    public static PropertyModel createDefaultModel() {
        return new PropertyModel.Builder(ALL_KEYS).build();
    }
}
