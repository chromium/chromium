// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.signin.services;

import android.util.Pair;

import androidx.annotation.AnyThread;
import androidx.annotation.IntDef;
import androidx.annotation.MainThread;
import androidx.annotation.VisibleForTesting;

import org.jni_zero.CalledByNative;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.components.metrics.LowEntropySource;
import org.chromium.components.variations.NormalizedMurmurHashEntropyProvider;
import org.chromium.components.version_info.Channel;
import org.chromium.components.version_info.VersionConstants;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * Creates a Field Trial to control the MobileIdentityConsistencyFRE feature. This feature is used
 * to show a new First Run page that will let users sign into chrome without syncing. The trial is
 * client controlled because this experiment runs on First Run Experience when native code is not
 * initialized and variation seed in not available.
 *
 * After creating a field trial the group information is saved in {@link ChromeSharedPreferences}
 * so that it's available in subsequent runs of Chrome.
 */
public class FREMobileIdentityConsistencyFieldTrial {
    private static final Object LOCK = new Object();

    /**
     * Used as a seed while selecting the group for the trial.
     */
    private static final int STUDY_RANDOMIZATION_SALT = 0xee9a496f;

    /**
     * The group variation values should be consecutive starting from zero. WELCOME_TO_CHROME acts
     * as the control group of the experiment.
     * If a new group needs to be added then another control group must also be added.
     */
    @VisibleForTesting
    @Retention(RetentionPolicy.SOURCE)
    @IntDef({
            VariationsGroup.DEFAULT,
            VariationsGroup.WELCOME_TO_CHROME,
            VariationsGroup.WELCOME_TO_CHROME_MOST_OUT_OF_CHROME,
            VariationsGroup.WELCOME_TO_CHROME_ADDITIONAL_FEATURES,
            VariationsGroup.WELCOME_TO_CHROME_EASIER_ACROSS_DEVICES,
            VariationsGroup.MOST_OUT_OF_CHROME,
            VariationsGroup.MAKE_CHROME_YOUR_OWN,
            VariationsGroup.MAX_VALUE,
    })
    public @interface VariationsGroup {
        /**
         * Default group of the experiment which is the group WELCOME_TO_CHROME.
         *
         * Title: 'Welcome to Chrome'
         * Subtitle: None
         */
        int DEFAULT = -1;
        /**
         * Title: 'Welcome to Chrome'
         * Subtitle: None
         */
        int WELCOME_TO_CHROME = 0;
        /**
         * Title: 'Welcome to Chrome'
         * Subtitle: 'Sign in to get the most out of Chrome'
         */
        int WELCOME_TO_CHROME_MOST_OUT_OF_CHROME = 1;
        /**
         * Title: 'Welcome to Chrome'
         * Subtitle: 'Sign in for additional features'
         */
        int WELCOME_TO_CHROME_ADDITIONAL_FEATURES = 2;
        /**
         * Title: 'Welcome to Chrome'
         * Subtitle: 'Sign in to browse easier across devices'
         */
        int WELCOME_TO_CHROME_EASIER_ACROSS_DEVICES = 3;
        /**
         * Title: 'Sign in to get the most out of Chrome'
         * Subtitle: None
         */
        int MOST_OUT_OF_CHROME = 4;
        /**
         * Title: 'Sign in to make Chrome your own'
         * Subtitle: None
         */
        int MAKE_CHROME_YOUR_OWN = 5;
        /**
         * When adding new groups, increasing this value will automatically cause new groups
         * to receive clients. A different control group will need to be implemented however
         * when adding new groups.
         */
        int MAX_VALUE = 6;
    }

    @AnyThread
    private static @VariationsGroup int getFirstRunTrialGroup() {
        synchronized (LOCK) {
            return ChromeSharedPreferences.getInstance().readInt(
                    ChromePreferenceKeys.FIRST_RUN_VARIATIONS_FIELD_TRIAL_GROUP,
                    VariationsGroup.DEFAULT);
        }
    }

    @CalledByNative
    @AnyThread
    private static int getFirstRunTrialVariationId(int lowEntropySource, int lowEntropySize) {
        @VariationsGroup
        int groupFromPrefs = getFirstRunTrialGroup();
        final int variationsEmptyID = 0; // This should be identical to variations::EMPTY_ID.
        if (groupFromPrefs == VariationsGroup.DEFAULT) {
            return variationsEmptyID; // Do not send variations ID if the user is the default group.
        }

        @VariationsGroup
        int groupFromLowEntropySource =
                generateFirstRunStringVariationsGroup(lowEntropySource, lowEntropySize);
        boolean isGroupConsistent = groupFromPrefs == groupFromLowEntropySource;
        RecordHistogram.recordBooleanHistogram(
                "Signin.AndroidIsFREStudyGroupConsistent", isGroupConsistent);

        if (!isGroupConsistent) {
            return variationsEmptyID; // Do not send variations ID if there's a mismatch.
        }
        if (VersionConstants.CHANNEL == Channel.STABLE) {
            // IDs in this method were obtained following go/finch-allocating-gws-ids.
            switch (groupFromPrefs) {
                case VariationsGroup.WELCOME_TO_CHROME:
                    return 3362112;
                case VariationsGroup.WELCOME_TO_CHROME_MOST_OUT_OF_CHROME:
                    return 3362113;
                case VariationsGroup.WELCOME_TO_CHROME_ADDITIONAL_FEATURES:
                    return 3362114;
                case VariationsGroup.WELCOME_TO_CHROME_EASIER_ACROSS_DEVICES:
                    return 3362115;
                case VariationsGroup.MOST_OUT_OF_CHROME:
                    return 3362116;
                case VariationsGroup.MAKE_CHROME_YOUR_OWN:
                    return 3362117;
                case VariationsGroup.MAX_VALUE:
                    return variationsEmptyID;
            }
        } else if (VersionConstants.CHANNEL == Channel.BETA) {
            switch (groupFromPrefs) {
                case VariationsGroup.WELCOME_TO_CHROME:
                    return 3362120;
                case VariationsGroup.WELCOME_TO_CHROME_MOST_OUT_OF_CHROME:
                    return 3362121;
                case VariationsGroup.WELCOME_TO_CHROME_ADDITIONAL_FEATURES:
                    return 3362122;
                case VariationsGroup.WELCOME_TO_CHROME_EASIER_ACROSS_DEVICES:
                    return 3362123;
                case VariationsGroup.MOST_OUT_OF_CHROME:
                    return 3362124;
                case VariationsGroup.MAKE_CHROME_YOUR_OWN:
                    return 3362125;
                case VariationsGroup.MAX_VALUE:
                    return variationsEmptyID;
            }
        }
        return variationsEmptyID; // In other channels, the experiment is not GWS-visible.
    }

    @CalledByNative
    @AnyThread
    public static String getFirstRunVariationsTrialGroupName() {
        final @VariationsGroup int group = getFirstRunTrialGroup();
        switch (group) {
            case VariationsGroup.WELCOME_TO_CHROME:
                return "Control";
            case VariationsGroup.WELCOME_TO_CHROME_MOST_OUT_OF_CHROME:
                return "WelcomeToChrome_MostOutOfChrome";
            case VariationsGroup.WELCOME_TO_CHROME_ADDITIONAL_FEATURES:
                return "WelcomeToChrome_StrongestSecurity";
            case VariationsGroup.WELCOME_TO_CHROME_EASIER_ACROSS_DEVICES:
                return "WelcomeToChrome_EasierAcrossDevices";
            case VariationsGroup.MOST_OUT_OF_CHROME:
                return "MostOutOfChrome";
            case VariationsGroup.MAKE_CHROME_YOUR_OWN:
                return "MakeChromeYourOwn";
            default:
                return "Default";
        }
    }

    /**
     * Returns different titles and subtitles depending upon the group variation.
     *
     * The first integer holds the string resource value for the title and the second integer
     * for the subtitle. The second integer may be 0, in which case there is no subtitle.
     */
    @MainThread
    public static Pair<Integer, Integer> getVariationTitleAndSubtitle() {
        final @VariationsGroup int group = getFirstRunTrialGroup();
        switch (group) {
            case VariationsGroup.WELCOME_TO_CHROME:
                return new Pair(R.string.fre_welcome, 0);
            case VariationsGroup.WELCOME_TO_CHROME_MOST_OUT_OF_CHROME:
                return new Pair(R.string.fre_welcome, R.string.signin_fre_subtitle_variation_1);
            case VariationsGroup.WELCOME_TO_CHROME_ADDITIONAL_FEATURES:
                return new Pair(R.string.fre_welcome, R.string.signin_fre_subtitle_variation_2);
            case VariationsGroup.WELCOME_TO_CHROME_EASIER_ACROSS_DEVICES:
                return new Pair(R.string.fre_welcome, R.string.signin_fre_subtitle_variation_3);
            case VariationsGroup.MOST_OUT_OF_CHROME:
                return new Pair(R.string.signin_fre_title_variation_1, 0);
            case VariationsGroup.MAKE_CHROME_YOUR_OWN:
                return new Pair(R.string.signin_fre_title_variation_2, 0);
            default:
                // By default VariationsGroup.WELCOME_TO_CHROME UI is shown in the fre.
                return new Pair(R.string.fre_welcome, 0);
        }
    }

    private static @VariationsGroup int generateFirstRunStringVariationsGroup(
            int lowEntropyValue, int lowEntropySize) {
        double variationsPercentage = 0;
        switch (VersionConstants.CHANNEL) {
            case Channel.DEFAULT:
            case Channel.CANARY:
            case Channel.DEV:
            case Channel.BETA:
                variationsPercentage = 10;
                break;
            case Channel.STABLE:
                variationsPercentage = 0.5;
                break;
        }
        // For A/B testing all experiment groups should have the same percentages.
        assert variationsPercentage * VariationsGroup.MAX_VALUE <= 100;

        NormalizedMurmurHashEntropyProvider entropyProvider =
                new NormalizedMurmurHashEntropyProvider(lowEntropyValue, lowEntropySize);
        double entropyForTrial = entropyProvider.getEntropyForTrial(STUDY_RANDOMIZATION_SALT);
        double randomBucket = entropyForTrial * 100;

        if (randomBucket < variationsPercentage * VariationsGroup.MAX_VALUE) {
            return (int) (randomBucket / variationsPercentage);
        }

        return VariationsGroup.DEFAULT;
    }

    /**
     * Returns whether the title and the subtitle should be hidden until native code and policies
     * are loaded on device.
     */
    @MainThread
    public static boolean shouldHideTitleUntilPoliciesAreLoaded() {
        final @VariationsGroup int group = getFirstRunTrialGroup();
        return group != VariationsGroup.DEFAULT && group != VariationsGroup.WELCOME_TO_CHROME;
    }

    /**
     * Creates variations of the FRE signin welcome screen with different title/subtitle
     * combinations.
     *
     * The group information is registered as a synthetic field trial in native code inside
     * ChromeBrowserFieldTrials::RegisterSyntheticTrials().
     */
    @MainThread
    public static void createFirstRunVariationsTrial() {
        // TODO(https://crbug.com/1430512): Add a test to verify that the group assignment stays
        // consistent when the user closes and reopens Chrome during the FRE.
        synchronized (LOCK) {
            // Don't create a new group if the user was already assigned a group. Can
            // happen when the user dismisses FRE without finishing the flow and starts chrome
            // again.
            if (ChromeSharedPreferences.getInstance().readInt(
                        ChromePreferenceKeys.FIRST_RUN_VARIATIONS_FIELD_TRIAL_GROUP, -2)
                    != -2) {
                return;
            }
        }
        int group = generateFirstRunStringVariationsGroup(
                LowEntropySource.generateLowEntropySourceForFirstRunTrial(),
                LowEntropySource.MAX_LOW_ENTROPY_SIZE);
        synchronized (LOCK) {
            ChromeSharedPreferences.getInstance().writeInt(
                    ChromePreferenceKeys.FIRST_RUN_VARIATIONS_FIELD_TRIAL_GROUP, group);
        }
    }

    @AnyThread
    public static void setFirstRunVariationsTrialGroupForTesting(@VariationsGroup int group) {
        synchronized (LOCK) {
            ChromeSharedPreferences.getInstance().writeInt(
                    ChromePreferenceKeys.FIRST_RUN_VARIATIONS_FIELD_TRIAL_GROUP, group);
        }
    }
}
