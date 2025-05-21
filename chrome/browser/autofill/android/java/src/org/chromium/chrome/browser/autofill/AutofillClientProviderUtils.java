// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill;

import static org.chromium.chrome.browser.preferences.ChromePreferenceKeys.AUTOFILL_THIRD_PARTY_MODE_STATE;

import android.content.ComponentName;
import android.content.Context;
import android.content.SharedPreferences.Editor;
import android.os.Build;
import android.view.autofill.AutofillManager;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;

import org.chromium.base.ContextUtils;
import org.chromium.base.ResettersForTesting;
import org.chromium.base.ServiceLoaderUtil;
import org.chromium.base.shared_preferences.SharedPreferencesManager;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.autofill.AndroidAutofillFeatures;
import org.chromium.components.autofill.AutofillManagerWrapper;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.user_prefs.UserPrefs;

/** Helper functions for using Android Autofill in Chrome. */
@NullMarked
@JNINamespace("autofill")
public class AutofillClientProviderUtils {
    private static final String TAG = "AutofillClientProviderUtils";

    public static final String AUTOFILL_OPTIONS_DEEP_LINK_SHARED_PREFS_FILE =
            "autofill_options_deep_link_shared_prefs_file";
    public static final String AUTOFILL_OPTIONS_DEEP_LINK_FEATURE_KEY =
            "AUTOFILL_OPTIONS_DEEP_LINK_FEATURE_KEY";
    private static final String AWG_COMPONENT_NAME =
            "com.google.android.gms/com.google.android.gms.autofill.service.AutofillService";
    private static @Nullable Integer sAndroidAutofillFrameworkAvailabilityForTesting;

    /**
     * Overrides the return value of {@link isAllowedToUseAndroidAutofillFramework} to the given
     * {@code availability} value until the tearDown calls the resetter. No manual teardown
     * required.
     *
     * @param availability The return value for tests.
     */
    public static void setAutofillAvailabilityToUseForTesting(
            @AndroidAutofillAvailabilityStatus Integer availability) {
        sAndroidAutofillFrameworkAvailabilityForTesting = availability;
        ResettersForTesting.register(() -> sAndroidAutofillFrameworkAvailabilityForTesting = null);
    }

    /**
     * Checks whether all conditions are met for using the Android Autofill framework in CCTs. It
     * simplifies the call to {@link getAndroidAutofillFrameworkAvailability}.
     *
     * @param profile A {@link Profile} which keeps the pref enabling this feature.
     * @return true iff CCTs should be constructed with support for Android Autofill.
     */
    public static boolean isAutofillEnabledForCct(Profile profile) {
        if (!AndroidAutofillFeatures.ANDROID_AUTOFILL_VIRTUAL_VIEW_STRUCTURE_ANDROID_IN_CCT
                .isEnabled()) {
            return false;
        }
        return AutofillClientProviderUtils.getAndroidAutofillFrameworkAvailability(
                        UserPrefs.get(profile))
                == AndroidAutofillAvailabilityStatus.AVAILABLE;
    }

    /**
     * Returns whether all conditions are met for using the Android Autofill framework in Chrome on
     * Android: The AutofillManager exists, is enabled, and its provider is not Autofill with
     * Google.
     *
     * @param profile A {@link PrefService} which keeps the pref enabling this feature.
     * @return {@link AndroidAutofillAvailabilityStatus.AVAILABLE} if Android Autofill can be used
     *     or a reason why it can't.
     */
    @CalledByNative
    public static int getAndroidAutofillFrameworkAvailability(PrefService prefs) {
        if (sAndroidAutofillFrameworkAvailabilityForTesting != null) {
            return sAndroidAutofillFrameworkAvailabilityForTesting;
        }
        if (!ChromeFeatureList.isEnabled(
                ChromeFeatureList.AUTOFILL_VIRTUAL_VIEW_STRUCTURE_ANDROID)) {
            // Technically correct. Not a useful status since the feature must be set.
            return AndroidAutofillAvailabilityStatus.SETTING_TURNED_OFF;
        }
        if (!prefs.getBoolean(Pref.AUTOFILL_THIRD_PARTY_PASSWORD_MANAGERS_ALLOWED)) {
            return AndroidAutofillAvailabilityStatus.NOT_ALLOWED_BY_POLICY;
        }
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.P) {
            return AndroidAutofillAvailabilityStatus.ANDROID_VERSION_TOO_OLD;
        }
        AutofillManager manager =
                ContextUtils.getApplicationContext().getSystemService(AutofillManager.class);
        if (manager == null) {
            return AndroidAutofillAvailabilityStatus.ANDROID_AUTOFILL_MANAGER_NOT_AVAILABLE;
        }
        if (!AutofillManagerWrapper.isAutofillSupported(manager)) {
            return AndroidAutofillAvailabilityStatus.ANDROID_AUTOFILL_NOT_SUPPORTED;
        }
        ComponentName componentName =
                AutofillManagerWrapper.getAutofillServiceComponentName(manager);
        if (componentName == null) {
            return AndroidAutofillAvailabilityStatus.UNKNOWN_ANDROID_AUTOFILL_SERVICE;
        }
        if (AWG_COMPONENT_NAME.equals(componentName.flattenToString())) {
            return AndroidAutofillAvailabilityStatus.ANDROID_AUTOFILL_SERVICE_IS_GOOGLE;
        }
        if (!prefs.getBoolean(Pref.AUTOFILL_USING_VIRTUAL_VIEW_STRUCTURE)) {
            return AndroidAutofillAvailabilityStatus.SETTING_TURNED_OFF;
        }
        return AndroidAutofillAvailabilityStatus.AVAILABLE;
    }

    @CalledByNative
    public static void setThirdPartyModePref(boolean usesPlatformAutofill) {
        SharedPreferencesManager prefManager = ChromeSharedPreferences.getInstance();
        prefManager.writeBoolean(AUTOFILL_THIRD_PARTY_MODE_STATE, usesPlatformAutofill);
    }

    @CalledByNative
    public static void unsetThirdPartyModePref() {
        SharedPreferencesManager prefManager = ChromeSharedPreferences.getInstance();
        prefManager.removeKey(AUTOFILL_THIRD_PARTY_MODE_STATE);
    }

    @CalledByNative
    public static void setAutofillOptionsDeepLinkPref(boolean featureOn) {
        Editor editor =
                ContextUtils.getApplicationContext()
                        .getSharedPreferences(
                                AUTOFILL_OPTIONS_DEEP_LINK_SHARED_PREFS_FILE, Context.MODE_PRIVATE)
                        .edit();
        editor.putBoolean(AUTOFILL_OPTIONS_DEEP_LINK_FEATURE_KEY, featureOn);
        editor.apply();
    }

    @CalledByNative
    public static String getTrialGroupForPackage() {
        AndroidAutofillAccessibilityFieldTrial fieldTrialImpl =
                ServiceLoaderUtil.maybeCreate(AndroidAutofillAccessibilityFieldTrial.class);
        return fieldTrialImpl != null
                ? fieldTrialImpl.getTrialGroupForPackage()
                : AndroidAutofillAccessibilityFieldTrial.AUTOFILL_VIA_A11Y_DEPRECATION_DEFAULT;
    }

    private AutofillClientProviderUtils() {}
}
