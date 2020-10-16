// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp.cards.promo;

import androidx.annotation.IntDef;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.browser.homepage.HomepageManager;
import org.chromium.chrome.browser.homepage.HomepagePolicyManager;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.SharedPreferencesManager;
import org.chromium.components.embedder_support.util.UrlUtilities;
import org.chromium.components.feature_engagement.FeatureConstants;
import org.chromium.components.feature_engagement.Tracker;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * Homepage promo related utils class.
 */
final class HomepagePromoUtils {
    /**
     * Possible user actions associated with homepage promo. Used for Histogram
     * "NewTabPage.Promo.HomepagePromo" recorded in {@link #recordHomepagePromoEvent(int)}.
     *
     * These values are corresponded with enum "AndroidHomepagePromoAction" and persisted to logs,
     * therefore should be treated as append only.
     */
    @IntDef({HomepagePromoAction.CREATED, HomepagePromoAction.SEEN, HomepagePromoAction.DISMISSED,
            HomepagePromoAction.ACCEPTED, HomepagePromoAction.UNDO})
    @Retention(RetentionPolicy.SOURCE)
    public @interface HomepagePromoAction {
        int CREATED = 0;
        int SEEN = 1;
        int DISMISSED = 2;
        int ACCEPTED = 3;
        int UNDO = 4;

        int TOTAL = 5;
    }

    private static final int MAX_IMPRESSION_SEEN = 20;

    // Do not instantiate.
    private HomepagePromoUtils() {}

    private static boolean sBypassHomepageEnabledForTests;
    private static boolean sBypassUrlIsNTPForTests;

    /**
     * @param tracker Tracker for feature engagement system. If the tracker is null, then ignore the
     *         feature engagement system.
     * @return True if HomepagePromo satisfied the condition to be created. False otherwise
     */
    static boolean shouldCreatePromo(@Nullable Tracker tracker) {
        boolean shouldCreateInternal = isHomepageEnabled()
                && !HomepagePolicyManager.isHomepageManagedByPolicy()
                && HomepageManager.getInstance().getPrefHomepageUseDefaultUri() && !isHomepageNTP()
                && !isPromoDismissedInSharedPreference();

        return shouldCreateInternal
                && (tracker == null
                        || tracker.shouldTriggerHelpUI(
                                FeatureConstants.HOMEPAGE_PROMO_CARD_FEATURE));
    }

    static boolean isPromoDismissedInSharedPreference() {
        return SharedPreferencesManager.getInstance().readBoolean(getDismissedKey(), false);
    }

    static void setPromoDismissedInSharedPreference(boolean isDismissed) {
        SharedPreferencesManager.getInstance().writeBoolean(getDismissedKey(), isDismissed);
    }

    static String getDismissedKey() {
        return ChromePreferenceKeys.PROMO_IS_DISMISSED.createKey(
                FeatureConstants.HOMEPAGE_PROMO_CARD_FEATURE);
    }

    static String getTimesSeenKey() {
        return ChromePreferenceKeys.PROMO_TIMES_SEEN.createKey(
                FeatureConstants.HOMEPAGE_PROMO_CARD_FEATURE);
    }

    // TODO(wenyufu): check finch param controlling whether the homepage needs to be enabled.
    private static boolean isHomepageEnabled() {
        return !sBypassHomepageEnabledForTests && HomepageManager.isHomepageEnabled();
    }

    private static boolean isHomepageNTP() {
        return !sBypassUrlIsNTPForTests && UrlUtilities.isNTPUrl(HomepageManager.getHomepageUri());
    }

    static void recordHomepagePromoEvent(@HomepagePromoAction int action) {
        RecordHistogram.recordEnumeratedHistogram(
                "NewTabPage.Promo.HomepagePromo", action, HomepagePromoAction.TOTAL);

        if (action == HomepagePromoAction.CREATED || action == HomepagePromoAction.UNDO) return;

        String timesSeenKey = getTimesSeenKey();
        int timesSeen = SharedPreferencesManager.getInstance().readInt(timesSeenKey, 0);

        if (action == HomepagePromoAction.SEEN) {
            SharedPreferencesManager.getInstance().writeInt(timesSeenKey, timesSeen + 1);
        } else if (action == HomepagePromoAction.ACCEPTED) {
            RecordHistogram.recordCountHistogram(
                    "NewTabPage.Promo.HomepagePromo.ImpressionUntilAction", timesSeen);
        } else if (action == HomepagePromoAction.DISMISSED) {
            RecordHistogram.recordLinearCountHistogram(
                    "NewTabPage.Promo.HomepagePromo.ImpressionUntilDismissal", timesSeen, 1,
                    MAX_IMPRESSION_SEEN, MAX_IMPRESSION_SEEN);
        }
    }

    @VisibleForTesting
    static void setBypassHomepageEnabledForTests(boolean doBypass) {
        sBypassHomepageEnabledForTests = doBypass;
    }

    @VisibleForTesting
    static void setBypassUrlIsNTPForTests(boolean doBypass) {
        sBypassUrlIsNTPForTests = doBypass;
    }
}
