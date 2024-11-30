// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.app.flags;

import android.text.TextUtils;

import androidx.annotation.AnyThread;
import androidx.annotation.VisibleForTesting;

import org.jni_zero.CalledByNative;

import org.chromium.base.ApplicationStatus;
import org.chromium.base.library_loader.LibraryLoader;
import org.chromium.build.BuildConfig;
import org.chromium.chrome.browser.firstrun.FirstRunUtils;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.components.browser_ui.modaldialog.ModalDialogFeatureMap;
import org.chromium.components.cached_flags.CachedFieldTrialParameter;
import org.chromium.components.cached_flags.CachedFlag;
import org.chromium.components.cached_flags.CachedFlagUtils;
import org.chromium.components.cached_flags.CachedFlagsSafeMode;
import org.chromium.components.omnibox.OmniboxFeatures;

import java.util.ArrayList;
import java.util.List;

/** Caches the flags that Chrome might require before native is loaded in a later next run. */
public class ChromeCachedFlags {
    private static final ChromeCachedFlags INSTANCE = new ChromeCachedFlags();
    static final List<List<CachedFlag>> LISTS_OF_CACHED_FLAGS_FULL_BROWSER =
            List.of(
                    ChromeFeatureList.sFlagsCachedFullBrowser,
                    OmniboxFeatures.getFieldTrialsToCache(),
                    ModalDialogFeatureMap.sCachedFlags);
    static final List<List<CachedFlag>> LISTS_OF_CACHED_FLAGS_MINIMAL_BROWSER =
            List.of(ChromeFeatureList.sFlagsCachedInMinimalBrowser);

    private boolean mIsFinishedCachingNativeFlags;

    /**
     * A list of field trial parameters that will be cached when starting minimal browser mode. See
     * {@link #cacheMinimalBrowserFlags()}.
     */
    private static final List<CachedFieldTrialParameter<?>> MINIMAL_BROWSER_FIELD_TRIALS =
            List.of();

    /**
     * @return The {@link ChromeCachedFlags} singleton.
     */
    public static ChromeCachedFlags getInstance() {
        return INSTANCE;
    }

    /**
     * Caches flags that are needed by Activities that launch before the native library is loaded
     * and stores them in SharedPreferences. Because this function is called during launch after the
     * library has loaded, any flags that have already been accessed won't reflect the most recent
     * server configuration state until the next launch after Chrome is restarted.
     */
    public void cacheNativeFlags() {
        if (mIsFinishedCachingNativeFlags) return;
        FirstRunUtils.cacheFirstRunPrefs();

        CachedFlagUtils.cacheNativeFlags(LISTS_OF_CACHED_FLAGS_FULL_BROWSER);
        cacheAdditionalNativeFlags();

        tryToCatchMissingParameters(
                ChromeFeatureList.sParamsCached, OmniboxFeatures.getFieldTrialParamsToCache());
        CachedFlagUtils.cacheFieldTrialParameters(
                ChromeFeatureList.sParamsCached, OmniboxFeatures.getFieldTrialParamsToCache());

        CachedFlagsSafeMode.getInstance().onEndCheckpoint();
        mIsFinishedCachingNativeFlags = true;
    }

    private void tryToCatchMissingParameters(
            List<CachedFieldTrialParameter<?>>... listsOfParamsToTest) {
        if (!BuildConfig.ENABLE_ASSERTS) return;

        var paramsToTest = new ArrayList<CachedFieldTrialParameter<?>>();
        for (List<CachedFieldTrialParameter<?>> list : listsOfParamsToTest) {
            paramsToTest.addAll(list);
        }

        // All instances of CachedFieldTrialParameter should be manually passed to
        // CachedFeatureFlags.cacheFieldTrialParameters(). The following checking is a best-effort
        // attempt to try to catch accidental omissions. It cannot replace the list because some
        // instances might not be instantiated if the classes they belong to are not accessed yet.
        List<String> omissions = new ArrayList<>();
        for (CachedFieldTrialParameter<?> trial : CachedFieldTrialParameter.getAllInstances()) {
            if (paramsToTest.contains(trial)) continue;
            if (MINIMAL_BROWSER_FIELD_TRIALS.contains(trial)) continue;
            omissions.add(trial.getFeatureName() + ":" + trial.getName());
        }
        assert omissions.isEmpty()
                : "The following trials are not correctly cached: "
                        + TextUtils.join(", ", omissions);
    }

    /**
     * Caches flags that are enabled in minimal browser mode and must take effect on startup but are
     * set via native code. This function needs to be called in minimal browser mode to mark these
     * field trials as active, otherwise histogram data recorded in minimal browser mode won't be
     * tagged with their corresponding field trial experiments.
     */
    public void cacheMinimalBrowserFlags() {
        cacheMinimalBrowserFlagsTimeFromNativeTime();
        CachedFlagUtils.cacheNativeFlags(LISTS_OF_CACHED_FLAGS_MINIMAL_BROWSER);
        CachedFlagUtils.cacheFieldTrialParameters(MINIMAL_BROWSER_FIELD_TRIALS);
    }

    /**
     * Caches a predetermined list of flags that must take effect on startup but are set via native
     * code.
     *
     * Do not add new simple boolean flags here, add them to {@link #cacheNativeFlags} instead.
     */
    public static void cacheAdditionalNativeFlags() {
        // Propagate the BACKGROUND_THREAD_POOL feature value to LibraryLoader.
        LibraryLoader.setBackgroundThreadPoolEnabledOnNextRuns(
                ChromeFeatureList.isEnabled(ChromeFeatureList.BACKGROUND_THREAD_POOL));

        // Propagate the CACHE_ACTIVITY_TASKID feature value to ApplicationStatus.
        ApplicationStatus.setCachingEnabled(
                ChromeFeatureList.isEnabled(ChromeFeatureList.CACHE_ACTIVITY_TASKID));
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    static void cacheMinimalBrowserFlagsTimeFromNativeTime() {
        ChromeSharedPreferences.getInstance()
                .writeLong(
                        ChromePreferenceKeys.FLAGS_LAST_CACHED_MINIMAL_BROWSER_FLAGS_TIME_MILLIS,
                        System.currentTimeMillis());
    }

    public static long getLastCachedMinimalBrowserFlagsTimeMillis() {
        return ChromeSharedPreferences.getInstance()
                .readLong(
                        ChromePreferenceKeys.FLAGS_LAST_CACHED_MINIMAL_BROWSER_FLAGS_TIME_MILLIS,
                        0);
    }

    @CalledByNative
    @AnyThread
    static boolean isEnabled(String featureName) {
        CachedFlag cachedFlag = ChromeFeatureList.sAllCachedFlags.get(featureName);
        assert cachedFlag != null;

        return cachedFlag.isEnabled();
    }
}
