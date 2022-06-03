// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp.cards.promo.enhanced_protection;

import androidx.annotation.IntDef;
import androidx.annotation.Nullable;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.preferences.SharedPreferencesManager;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.safe_browsing.SafeBrowsingBridge;
import org.chromium.components.user_prefs.UserPrefs;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * Enhanced Protection promo related utils class.
 */
final class EnhancedProtectionPromoUtils {
    /**
     * Possible user actions associated with homepage promo. Used for Histogram
     * "NewTabPage.Promo.EnhancedProtectionPromo" recorded in {@link
     * #recordEnhancedProtectionPromoEvent(int)}.
     *
     * These values are corresponded with enum "AndroidEnhancedProtectionPromoAction" and persisted
     * to logs, therefore should be treated as append only.
     */
    @IntDef({EnhancedProtectionPromoAction.CREATED, EnhancedProtectionPromoAction.SEEN,
            EnhancedProtectionPromoAction.DISMISSED, EnhancedProtectionPromoAction.ACCEPTED})
    @Retention(RetentionPolicy.SOURCE)
    public @interface EnhancedProtectionPromoAction {
        int CREATED = 0;
        int SEEN = 1;
        int DISMISSED = 2;
        int ACCEPTED = 3;
        int TOTAL = 4;
    }

    // Suffix for PROMO_IS_DISMISSED and PROMO_TIMES_SEEN Chrome preference keys.
    public static final String ENHANCED_PROTECTION_PROMO_CARD_FEATURE =
            "EnhancedProtectionPromoCard";
    private static final int DEFAULT_MAX_IMPRESSION_SEEN = 22;

    // Do not instantiate.
    private EnhancedProtectionPromoUtils() {}

    /**
     * @return True if EnhancedProtectionPromo has not yet been dismissed and if the user has not
     *         selected Enhanced Protection and if the user has not seen the promos too many times
     *         and if Safe Browsing is not managed.
     */
    static boolean shouldCreatePromo(@Nullable Profile profile) {
        String timesSeenKey = getTimesSeenKey();
        int timesSeen = SharedPreferencesManager.getInstance().readInt(timesSeenKey, 0);
        int maxImpressions = ChromeFeatureList.getFieldTrialParamByFeatureAsInt(
                ChromeFeatureList.ENHANCED_PROTECTION_PROMO_CARD,
                "MaxEnhancedProtectionPromoImpressions", DEFAULT_MAX_IMPRESSION_SEEN);
        // TODO(bdea): If the user has pressed "Continue" and not selected Enhanced Protection,
        // should we still show the promo.
        return (profile != null) && !UserPrefs.get(profile).getBoolean(Pref.SAFE_BROWSING_ENHANCED)
                && !isPromoDismissedInSharedPreference() && (timesSeen <= maxImpressions)
                && !SafeBrowsingBridge.isSafeBrowsingManaged();
    }

    static boolean isPromoDismissedInSharedPreference() {
        return SharedPreferencesManager.getInstance().readBoolean(getDismissedKey(), false);
    }

    static void setPromoDismissedInSharedPreference(boolean isDismissed) {
        SharedPreferencesManager.getInstance().writeBoolean(getDismissedKey(), isDismissed);
    }

    static String getDismissedKey() {
        return ChromePreferenceKeys.PROMO_IS_DISMISSED.createKey(
                ENHANCED_PROTECTION_PROMO_CARD_FEATURE);
    }

    static String getTimesSeenKey() {
        return ChromePreferenceKeys.PROMO_TIMES_SEEN.createKey(
                ENHANCED_PROTECTION_PROMO_CARD_FEATURE);
    }

    static void recordEnhancedProtectionPromoEvent(@EnhancedProtectionPromoAction int action) {
        RecordHistogram.recordEnumeratedHistogram("NewTabPage.Promo.EnhancedProtectionPromo",
                action, EnhancedProtectionPromoAction.TOTAL);

        if (action == EnhancedProtectionPromoAction.CREATED) return;
        int maxImpressions = ChromeFeatureList.getFieldTrialParamByFeatureAsInt(
                ChromeFeatureList.ENHANCED_PROTECTION_PROMO_CARD,
                "MaxEnhancedProtectionPromoImpressions", DEFAULT_MAX_IMPRESSION_SEEN);
        String timesSeenKey = getTimesSeenKey();
        int timesSeen = SharedPreferencesManager.getInstance().readInt(timesSeenKey, 0);
        if (action == EnhancedProtectionPromoAction.SEEN) {
            SharedPreferencesManager.getInstance().writeInt(timesSeenKey, timesSeen + 1);
        } else if (action == EnhancedProtectionPromoAction.ACCEPTED) {
            RecordUserAction.record("NewTabPage.Promo.EnhancedProtectionPromo.Accepted");
            RecordHistogram.recordLinearCountHistogram(
                    "NewTabPage.Promo.EnhancedProtectionPromo.ImpressionUntilAction", timesSeen, 1,
                    maxImpressions, maxImpressions + 1);
        } else if (action == EnhancedProtectionPromoAction.DISMISSED) {
            RecordUserAction.record("NewTabPage.Promo.EnhancedProtectionPromo.Dismissed");
            RecordHistogram.recordLinearCountHistogram(
                    "NewTabPage.Promo.EnhancedProtectionPromo.ImpressionUntilDismissal", timesSeen,
                    1, maxImpressions, maxImpressions + 1);
        }
    }
}
