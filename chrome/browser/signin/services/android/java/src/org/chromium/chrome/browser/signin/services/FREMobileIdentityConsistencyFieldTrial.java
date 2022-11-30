// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.signin.services;

import android.util.Pair;

import androidx.annotation.AnyThread;
import androidx.annotation.IntDef;
import androidx.annotation.MainThread;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.CommandLine;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.SharedPreferencesManager;
import org.chromium.components.metrics.LowEntropySource;
import org.chromium.components.variations.NormalizedMurmurHashEntropyProvider;
import org.chromium.components.version_info.Channel;
import org.chromium.components.version_info.VersionConstants;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.Random;

/**
 * Creates a Field Trial to control the MobileIdentityConsistencyFRE feature. This feature is used
 * to show a new First Run page that will let users sign into chrome without syncing. The trial is
 * client controlled because this experiment runs on First Run Experience when native code is not
 * initialized and variation seed in not available.
 *
 * After creating a field trial the group information is saved in {@link SharedPreferencesManager}
 * so that it's available in subsequent runs of Chrome.
 */
public class FREMobileIdentityConsistencyFieldTrial {
    private static final Object LOCK = new Object();
    private static final String ENABLED_GROUP = "Enabled9";
    @VisibleForTesting
    public static final String DISABLED_GROUP = "Disabled9";
    private static final String DEFAULT_GROUP = "Default";
    @VisibleForTesting
    public static final String OLD_FRE_WITH_UMA_DIALOG_GROUP = "OldFreWithUmaDialog9";

    /**
     * Shows the new flow with separate sign-in and sync pages. Uses the new initialization logic
     * that doesn't require native initialization to be finished for showing continue/dismiss
     * buttons on the welcome screen.
     */
    @VisibleForTesting
    public static final String INITIALIZATION_FLOW_NEW_GROUP = "InitializationFlowNew9";
    /**
     * Shows the new flow with separate sign-in and sync pages. Uses the old initialization logic
     * in which continue/dismiss buttons on the welcome screen are shown after native is
     * initialized. This group is used to check the effect of the delay introduced by the native
     * initialization.
     */
    public static final String INITIALIZATION_FLOW_OLD_GROUP = "InitializationFlowOld9";
    /**
     * Shows the flow without the sign-in page but with UMA controls in a dialog.
     * This group is used as control for {@link #INITIALIZATION_FLOW_NEW_GROUP} and
     * {@link #INITIALIZATION_FLOW_OLD_GROUP}.
     */
    private static final String INITIALIZATION_FLOW_CONTROL_GROUP = "InitializationFlowControl9";
    /**
     * Used as a seed while selecting the group for the trial.
     */
    private static final int STUDY_RANDOMIZATION_SALT = 0xf2689bf8;

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
            VariationsGroup.WELCOME_TO_CHROME_STRONGEST_SECURITY,
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
         * Subtitle: 'Sign in for additional features and Chrome's strongest security'
         */
        int WELCOME_TO_CHROME_STRONGEST_SECURITY = 2;
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
    public static boolean isEnabled() {
        // Switch used for tests. Disabled by default otherwise.
        if (CommandLine.getInstance().hasSwitch(ChromeSwitches.FORCE_DISABLE_SIGNIN_FRE)) {
            return false;
        }
        if (CommandLine.getInstance().hasSwitch(ChromeSwitches.FORCE_ENABLE_SIGNIN_FRE)) {
            return true;
        }
        if (getFirstRunTrialGroup().equals(INITIALIZATION_FLOW_NEW_GROUP)
                || getFirstRunTrialGroup().equals(INITIALIZATION_FLOW_OLD_GROUP)) {
            return true;
        }

        // Group names were changed from 'Enabled' to 'Enabled2' starting from Beta experiment.
        // getFirstRunTrialGroup.startWith() matches old groups alongside new groups.
        return getFirstRunTrialGroup().startsWith("Enabled");
    }

    @MainThread
    public static boolean shouldShowOldFreWithUmaDialog() {
        // Switch used for tests.
        if (CommandLine.getInstance().hasSwitch(ChromeSwitches.FORCE_DISABLE_SIGNIN_FRE)) {
            return false;
        }
        return OLD_FRE_WITH_UMA_DIALOG_GROUP.equals(getFirstRunTrialGroup())
                || INITIALIZATION_FLOW_CONTROL_GROUP.equals(getFirstRunTrialGroup());
    }

    @MainThread
    public static boolean shouldUseNewInitializationFlow() {
        if (CommandLine.getInstance().hasSwitch(ChromeSwitches.FORCE_DISABLE_SIGNIN_FRE)) {
            return false;
        }
        return getFirstRunTrialGroup().equals(INITIALIZATION_FLOW_NEW_GROUP);
    }

    @CalledByNative
    @AnyThread
    public static String getFirstRunTrialGroup() {
        synchronized (LOCK) {
            return SharedPreferencesManager.getInstance().readString(
                    ChromePreferenceKeys.FIRST_RUN_FIELD_TRIAL_GROUP, DEFAULT_GROUP);
        }
    }

    @CalledByNative
    @AnyThread
    private static int getFirstRunTrialVariationId(int lowEntropySource, int lowEntropySize) {
        String groupFromLowEntropySource =
                generateFirstRunTrialGroup(lowEntropySource, lowEntropySize);
        boolean isGroupConsistent = getFirstRunTrialGroup().equals(groupFromLowEntropySource);
        RecordHistogram.recordBooleanHistogram(
                "Signin.AndroidIsFREStudyGroupConsistent", isGroupConsistent);

        final int variationsEmptyID = 0; // This should be identical to variations::EMPTY_ID.
        if (!isGroupConsistent) {
            return variationsEmptyID; // Do not send variations ID if there's a mismatch.
        }
        if (VersionConstants.CHANNEL == Channel.STABLE) {
            // IDs in this method were obtained following go/finch-allocating-gws-ids.
            switch (getFirstRunTrialGroup()) {
                case DISABLED_GROUP:
                    return 3354002;
                case ENABLED_GROUP:
                    return 3354003;
                case OLD_FRE_WITH_UMA_DIALOG_GROUP:
                    return 3354004;
                case INITIALIZATION_FLOW_CONTROL_GROUP:
                    return 3356513;
                case INITIALIZATION_FLOW_NEW_GROUP:
                    return 3356514;
                case INITIALIZATION_FLOW_OLD_GROUP:
                    return 3356515;
                default:
                    break;
            }
        } else if (VersionConstants.CHANNEL == Channel.BETA) {
            switch (getFirstRunTrialGroup()) {
                case DISABLED_GROUP:
                    return 3356552;
                case ENABLED_GROUP:
                    return 3356553;
                case OLD_FRE_WITH_UMA_DIALOG_GROUP:
                    return 3356554;
                case INITIALIZATION_FLOW_CONTROL_GROUP:
                    return 3356555;
                case INITIALIZATION_FLOW_NEW_GROUP:
                    return 3356556;
                case INITIALIZATION_FLOW_OLD_GROUP:
                    return 3356557;
                default:
                    break;
            }
        }
        return variationsEmptyID; // In other channels, the experiment is not GWS-visible.
    }

    @AnyThread
    public static String getFirstRunVariationsTrialGroup() {
        @VariationsGroup
        final int group = getFirstRunVariationsTrialGroupInternal();
        switch (group) {
            case VariationsGroup.WELCOME_TO_CHROME:
                return "Control";
            case VariationsGroup.WELCOME_TO_CHROME_MOST_OUT_OF_CHROME:
                return "WelcomeToChrome_MostOutOfChrome";
            case VariationsGroup.WELCOME_TO_CHROME_STRONGEST_SECURITY:
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
        @VariationsGroup
        final int group = getFirstRunVariationsTrialGroupInternal();
        switch (group) {
            case VariationsGroup.WELCOME_TO_CHROME:
                return new Pair(R.string.fre_welcome, 0);
            case VariationsGroup.WELCOME_TO_CHROME_MOST_OUT_OF_CHROME:
                return new Pair(R.string.fre_welcome, R.string.signin_fre_subtitle_variation_1);
            case VariationsGroup.WELCOME_TO_CHROME_STRONGEST_SECURITY:
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

    /**
     * This method should be only called once during FRE in
     * {@link org.chromium.chrome.browser.firstrun.FirstRunActivity} so that subsequent chrome runs
     * don't override FRE experiment group information.
     *
     * FRE is launched either after first install of chrome or after a power wash. So there is be no
     * previous experiment group information available when this method is called.
     *
     * The group information is registered as a synthetic field trial in native code inside
     * ChromeBrowserFieldTrials::RegisterSyntheticTrials().
     */
    @MainThread
    public static void createFirstRunTrial() {
        synchronized (LOCK) {
            // Don't create a new group if the user was already assigned a group. Can happen when
            // the user dismisses FRE without finishing the flow and cold starts chrome again.
            if (SharedPreferencesManager.getInstance().readString(
                        ChromePreferenceKeys.FIRST_RUN_FIELD_TRIAL_GROUP, null)
                    != null) {
                return;
            }
        }

        String group = generateFirstRunTrialGroup(
                LowEntropySource.generateLowEntropySourceForFirstRunTrial(),
                LowEntropySource.MAX_LOW_ENTROPY_SIZE);
        synchronized (LOCK) {
            SharedPreferencesManager.getInstance().writeString(
                    ChromePreferenceKeys.FIRST_RUN_FIELD_TRIAL_GROUP, group);
        }
    }

    private static String generateFirstRunTrialGroup(int lowEntropyValue, int lowEntropySize) {
        // Tweak these values for different builds to create the percentage of group population.
        // For A/B testing all 3 experiment groups should have the same percentages.
        int enabledPercent = 0;
        int disabledPercent = 0;
        int oldFreWithUmaDialogPercent = 0;
        int initializationFlowNewPercent = 0;
        int initializationFlowOldPercent = 0;
        int initializationFlowControlPercent = 0;
        switch (VersionConstants.CHANNEL) {
            case Channel.DEFAULT:
            case Channel.CANARY:
            case Channel.DEV:
                enabledPercent = 16;
                disabledPercent = 16;
                oldFreWithUmaDialogPercent = 16;
                initializationFlowNewPercent = 16;
                initializationFlowOldPercent = 16;
                initializationFlowControlPercent = 16;
                break;
            case Channel.BETA:
                enabledPercent = 10;
                disabledPercent = 10;
                oldFreWithUmaDialogPercent = 10;
                initializationFlowNewPercent = 10;
                initializationFlowOldPercent = 10;
                initializationFlowControlPercent = 10;
                break;
            case Channel.STABLE:
                enabledPercent = 30;
                disabledPercent = 30;
                oldFreWithUmaDialogPercent = 30;
                initializationFlowNewPercent = 1;
                initializationFlowOldPercent = 1;
                initializationFlowControlPercent = 1;
                break;
        }
        assert enabledPercent + disabledPercent + oldFreWithUmaDialogPercent
                        + initializationFlowNewPercent + initializationFlowOldPercent
                        + initializationFlowControlPercent
                <= 100;

        NormalizedMurmurHashEntropyProvider entropyProvider =
                new NormalizedMurmurHashEntropyProvider(lowEntropyValue, lowEntropySize);
        double entropyForTrial = entropyProvider.getEntropyForTrial(STUDY_RANDOMIZATION_SALT);
        double randomBucket = entropyForTrial * 100;

        if (randomBucket < enabledPercent) return ENABLED_GROUP;
        randomBucket -= enabledPercent;

        if (randomBucket < disabledPercent) return DISABLED_GROUP;
        randomBucket -= disabledPercent;

        if (randomBucket < oldFreWithUmaDialogPercent) return OLD_FRE_WITH_UMA_DIALOG_GROUP;
        randomBucket -= oldFreWithUmaDialogPercent;

        if (randomBucket < initializationFlowNewPercent) return INITIALIZATION_FLOW_NEW_GROUP;
        randomBucket -= initializationFlowNewPercent;

        if (randomBucket < initializationFlowOldPercent) return INITIALIZATION_FLOW_OLD_GROUP;
        randomBucket -= initializationFlowOldPercent;

        if (randomBucket < initializationFlowControlPercent) {
            return INITIALIZATION_FLOW_CONTROL_GROUP;
        }
        return DEFAULT_GROUP;
    }

    /**
     * Returns whether the title and the subtitle should be hidden until native code and policies
     * are loaded on device.
     */
    @MainThread
    public static boolean shouldHideTitleUntilPoliciesAreLoaded() {
        @VariationsGroup
        final int group = getFirstRunVariationsTrialGroupInternal();
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
    private static void createFirstRunVariationsTrial() {
        int variationsPercentage = 0;
        switch (VersionConstants.CHANNEL) {
            case Channel.DEFAULT:
            case Channel.CANARY:
            case Channel.DEV:
                variationsPercentage = 10;
                break;
            case Channel.BETA:
            case Channel.STABLE:
        }
        // For A/B testing all experiment groups should have the same percentages.
        assert variationsPercentage * VariationsGroup.MAX_VALUE <= 100;

        final int randomBucket = new Random().nextInt(100);
        int group = VariationsGroup.DEFAULT;
        if (randomBucket < variationsPercentage * VariationsGroup.MAX_VALUE) {
            group = randomBucket / variationsPercentage;
        }
        synchronized (LOCK) {
            SharedPreferencesManager.getInstance().writeInt(
                    ChromePreferenceKeys.FIRST_RUN_VARIATIONS_FIELD_TRIAL_GROUP, group);
        }
    }

    @AnyThread
    private static int getFirstRunVariationsTrialGroupInternal() {
        synchronized (LOCK) {
            return SharedPreferencesManager.getInstance().readInt(
                    ChromePreferenceKeys.FIRST_RUN_VARIATIONS_FIELD_TRIAL_GROUP,
                    VariationsGroup.DEFAULT);
        }
    }

    @AnyThread
    public static void setFirstRunTrialGroupForTesting(String group) {
        synchronized (LOCK) {
            SharedPreferencesManager.getInstance().writeString(
                    ChromePreferenceKeys.FIRST_RUN_FIELD_TRIAL_GROUP, group);
        }
    }

    @AnyThread
    public static void setFirstRunVariationsTrialGroupForTesting(@VariationsGroup int group) {
        synchronized (LOCK) {
            SharedPreferencesManager.getInstance().writeInt(
                    ChromePreferenceKeys.FIRST_RUN_VARIATIONS_FIELD_TRIAL_GROUP, group);
        }
    }
}
