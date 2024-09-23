// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;


import android.content.Context;
import android.content.Intent;

import androidx.annotation.Nullable;

import org.chromium.base.IntentUtils;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.IntentHandler.IncognitoCCTCallerId;
import org.chromium.chrome.browser.browserservices.intents.CustomButtonParams;
import org.chromium.chrome.browser.flags.ChromeFeatureList;

import java.util.Collections;
import java.util.List;

/**
 * A model class that parses the incoming intent for Ephemeral Custom Tab specific customization
 * data.
 *
 * <p>Lifecycle: is activity-scoped, i.e. one instance per CustomTabActivity instance. Must be
 * re-created when color scheme changes, which happens automatically since color scheme change leads
 * to activity re-creation.
 */
public class EphemeralCustomTabIntentDataProvider extends CustomTabIntentDataProvider {
    /** Constructs an {@link EphemeralCustomTabIntentDataProvider}. */
    public EphemeralCustomTabIntentDataProvider(Intent intent, Context context, int colorScheme) {
        super(intent, context, colorScheme);
        assert isOffTheRecord();
        logFeatureUsage();
    }

    /**
     * Logs the usage of ephemeral CCT features to a large enum histogram in order to track usage by
     * apps.
     */
    private void logFeatureUsage() {
        if (!CustomTabsFeatureUsage.isEnabled()) return;
        CustomTabsFeatureUsage featureUsage = new CustomTabsFeatureUsage();

        // Ordering: Log all the features ordered by enum, when they apply.
        if (getCustomTabMode() == CustomTabProfileType.EPHEMERAL) {
            featureUsage.log(
                    CustomTabsFeatureUsage.CustomTabsFeature.EXTRA_ENABLE_EPHEMERAL_BROWSING);
        }
    }

    private static boolean isEphemeralTabRequested(Intent intent) {
        if (!ChromeFeatureList.sCctEphemeralMode.isEnabled()) return false;
        return IntentUtils.safeGetBooleanExtra(
                intent, IntentHandler.EXTRA_ENABLE_EPHEMERAL_BROWSING, false);
    }

    public @IntentHandler.IncognitoCCTCallerId int getFeatureIdForMetricsCollection() {
        return IncognitoCCTCallerId.EPHEMERAL_TAB;
    }

    public static boolean isValidEphemeralTabIntent(Intent intent) {
        return isEphemeralTabRequested(intent);
    }

    @Override
    public @CustomTabProfileType int getCustomTabMode() {
        return CustomTabProfileType.EPHEMERAL;
    }

    @Override
    public List<CustomButtonParams> getCustomButtonsOnGoogleBottomBar() {
        return Collections.emptyList();
    }

    @Override
    public @Nullable String getClientPackageNameIdentitySharing() {
        return null;
    }

    @Override
    public boolean isInteractiveOmniboxAllowed() {
        return false;
    }

    @Override
    public boolean shouldShowDownloadButton() {
        // TODO(crbug.com/335609494): Enable once Offline downloads is supported for OTR profiles.
        return false;
    }
}
