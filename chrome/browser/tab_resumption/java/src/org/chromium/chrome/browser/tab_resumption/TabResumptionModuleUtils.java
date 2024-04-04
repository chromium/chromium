// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_resumption;

import android.content.res.Resources;
import android.text.TextUtils;

import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.magic_stack.HomeModulesConfigManager;
import org.chromium.chrome.browser.magic_stack.ModuleDelegate.ModuleType;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.sync.SyncServiceFactory;
import org.chromium.chrome.browser.tab_resumption.TabResumptionModuleMetricsUtils.ModuleNotShownReason;
import org.chromium.chrome.browser.tab_resumption.TabResumptionModuleMetricsUtils.ModuleVisibility;
import org.chromium.components.embedder_support.util.UrlUtilities;
import org.chromium.components.signin.identitymanager.ConsentLevel;
import org.chromium.url.GURL;

import java.util.concurrent.TimeUnit;

/** Utilities for the tab resumption module. */
public class TabResumptionModuleUtils {

    /** Callback to handle click on suggestion tiles. */
    public interface SuggestionClickCallback {
        void onSuggestionClick(GURL gurl);
    }

    /**
     * Based on settings, decides whether Chrome should attempt to show the tab resumption module.
     * Provides the reason if the decision is to not show.
     */
    static ModuleVisibility computeModuleVisibility(Profile profile) {
        if (!ChromeFeatureList.sTabResumptionModuleAndroid.isEnabled()
                || !HomeModulesConfigManager.getInstance()
                        .getPrefModuleTypeEnabled(ModuleType.TAB_RESUMPTION)) {
            return new ModuleVisibility(false, ModuleNotShownReason.FEATURE_DISABLED);
        }

        if (!IdentityServicesProvider.get()
                .getIdentityManager(profile)
                .hasPrimaryAccount(ConsentLevel.SYNC)) {
            return new ModuleVisibility(false, ModuleNotShownReason.NOT_SIGNED_IN);
        }

        if (!SyncServiceFactory.getForProfile(profile).hasKeepEverythingSynced()) {
            return new ModuleVisibility(false, ModuleNotShownReason.NOT_SYNC);
        }

        return new ModuleVisibility(true, ModuleNotShownReason.NUM_ENTRIES);
    }

    /**
     * Returns whether to show the tab resumption module. The module shows only if the following are
     * met:
     *
     * <pre>
     * 1. Feature (by flag TAB_RESUMPTION_MODULE_ANDROID and user preferences) is enabled;
     * 2. The user has signed in;
     * 3. The user has turned on sync.
     * </pre>
     */
    static boolean shouldShowTabResumptionModule(Profile profile) {
        return computeModuleVisibility(profile).value;
    }

    /**
     * Computes the string representation of how recent an event was, given the time delta.
     *
     * @param res Resources for string resource retrieval.
     * @param timeDelta Time delta in milliseconds.
     */
    static String getRecencyString(Resources res, long timeDeltaMs) {
        if (timeDeltaMs < 0L) timeDeltaMs = 0L;

        long daysElapsed = TimeUnit.MILLISECONDS.toDays(timeDeltaMs);
        if (daysElapsed > 0L) {
            return res.getQuantityString(R.plurals.n_days_ago, (int) daysElapsed, daysElapsed);
        }

        long hoursElapsed = TimeUnit.MILLISECONDS.toHours(timeDeltaMs);
        if (hoursElapsed > 0L) {
            return res.getQuantityString(
                    R.plurals.n_hours_ago_narrow, (int) hoursElapsed, hoursElapsed);
        }

        // Bound recency to 1 min.
        long minutesElapsed = Math.max(1L, TimeUnit.MILLISECONDS.toMinutes(timeDeltaMs));
        return res.getQuantityString(
                R.plurals.n_minutes_ago_narrow, (int) minutesElapsed, minutesElapsed);
    }

    /**
     * Extracts the registered, organization-identifying host and all its registry information, but
     * no subdomains, from a given URL. In particular, removes the "www." prefix.
     */
    static String getDomainUrl(GURL url) {
        String domainUrl = UrlUtilities.getDomainAndRegistry(url.getSpec(), false);
        return TextUtils.isEmpty(domainUrl) ? url.getHost() : domainUrl;
    }
}
