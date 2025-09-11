// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;

import static androidx.browser.customtabs.CustomTabsIntent.EXTRA_ENABLE_EPHEMERAL_BROWSING;

import android.content.Context;
import android.content.Intent;

import org.chromium.base.IntentUtils;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.browserservices.intents.CustomButtonParams;

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
@NullMarked
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
        CustomTabsFeatureUsage featureUsage = new CustomTabsFeatureUsage();

        // Ordering: Log all the features ordered by enum, when they apply.
        if (getCustomTabMode() == CustomTabProfileType.EPHEMERAL) {
            featureUsage.log(
                    CustomTabsFeatureUsage.CustomTabsFeature.EXTRA_ENABLE_EPHEMERAL_BROWSING);
        }
    }

    private static boolean isEphemeralTabRequested(Intent intent) {
        return IntentUtils.safeGetBooleanExtra(intent, EXTRA_ENABLE_EPHEMERAL_BROWSING, false);
    }

    @Override
    public @IncognitoCctCallerId int getFeatureIdForMetricsCollection() {
        return IncognitoCctCallerId.EPHEMERAL_TAB;
    }

    public static boolean isValidEphemeralTabIntent(Intent intent) {
        return isEphemeralTabRequested(intent);
    }

    @Override
    public boolean isOptionalButtonSupported() {
        return false;
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
