// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.safe_browsing.settings;

import android.content.Context;
import android.os.Bundle;

import androidx.annotation.VisibleForTesting;
import androidx.preference.Preference;

import org.chromium.base.IntentUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.safe_browsing.SafeBrowsingBridge;
import org.chromium.chrome.browser.safe_browsing.SafeBrowsingState;
import org.chromium.chrome.browser.safe_browsing.metrics.SettingsAccessPoint;
import org.chromium.chrome.browser.safe_browsing.metrics.UserAction;
import org.chromium.chrome.browser.settings.ChromeManagedPreferenceDelegate;
import org.chromium.chrome.browser.settings.SettingsNavigationFactory;
import org.chromium.components.browser_ui.settings.ManagedPreferenceDelegate;

/** Fragment containing Safe Browsing settings. */
public class SafeBrowsingSettingsFragment extends SafeBrowsingSettingsFragmentBase
        implements RadioButtonGroupSafeBrowsingPreference.OnSafeBrowsingModeDetailsRequested,
                Preference.OnPreferenceChangeListener {
    @VisibleForTesting static final String PREF_MANAGED_DISCLAIMER_TEXT = "managed_disclaimer_text";
    @VisibleForTesting static final String PREF_SAFE_BROWSING = "safe_browsing_radio_button_group";
    public static final String ACCESS_POINT = "SafeBrowsingSettingsFragment.AccessPoint";

    private RadioButtonGroupSafeBrowsingPreference mSafeBrowsingPreference;
    private @SettingsAccessPoint int mAccessPoint;

    /**
     * @return A summary that describes the current Safe Browsing state.
     */
    public static String getSafeBrowsingSummaryString(Context context, Profile profile) {
        @SafeBrowsingState
        int safeBrowsingState = new SafeBrowsingBridge(profile).getSafeBrowsingState();
        String safeBrowsingStateString = "";
        if (safeBrowsingState == SafeBrowsingState.ENHANCED_PROTECTION) {
            safeBrowsingStateString =
                    context.getString(R.string.safe_browsing_enhanced_protection_title);
        } else if (safeBrowsingState == SafeBrowsingState.STANDARD_PROTECTION) {
            safeBrowsingStateString =
                    context.getString(R.string.safe_browsing_standard_protection_title);
        } else if (safeBrowsingState == SafeBrowsingState.NO_SAFE_BROWSING) {
            return context.getString(R.string.prefs_safe_browsing_no_protection_summary);
        } else {
            assert false : "Should not be reached";
        }
        return context.getString(R.string.prefs_safe_browsing_summary, safeBrowsingStateString);
    }

    /**
     * Creates an argument bundle to open the Safe Browsing settings page.
     * @param accessPoint The access point for opening the Safe Browsing settings page.
     */
    public static Bundle createArguments(@SettingsAccessPoint int accessPoint) {
        Bundle result = new Bundle();
        result.putInt(ACCESS_POINT, accessPoint);
        return result;
    }

    @Override
    protected void onCreatePreferencesInternal(Bundle bundle, String s) {
        mAccessPoint =
                IntentUtils.safeGetInt(getArguments(), ACCESS_POINT, SettingsAccessPoint.DEFAULT);

        ManagedPreferenceDelegate managedPreferenceDelegate = createManagedPreferenceDelegate();

        mSafeBrowsingPreference = findPreference(PREF_SAFE_BROWSING);
        mSafeBrowsingPreference.init(getSafeBrowsingBridge().getSafeBrowsingState(), mAccessPoint);
        mSafeBrowsingPreference.setSafeBrowsingModeDetailsRequestedListener(this);
        mSafeBrowsingPreference.setManagedPreferenceDelegate(managedPreferenceDelegate);
        mSafeBrowsingPreference.setOnPreferenceChangeListener(this);

        findPreference(PREF_MANAGED_DISCLAIMER_TEXT)
                .setVisible(
                        managedPreferenceDelegate.isPreferenceClickDisabled(
                                mSafeBrowsingPreference));

        recordUserActionHistogram(UserAction.SHOWED);
    }

    @Override
    protected int getPreferenceResource() {
        return R.xml.safe_browsing_preferences;
    }

    @Override
    public void onSafeBrowsingModeDetailsRequested(@SafeBrowsingState int safeBrowsingState) {
        recordUserActionHistogramForStateDetailsClicked(safeBrowsingState);
        if (safeBrowsingState == SafeBrowsingState.ENHANCED_PROTECTION) {
            SettingsNavigationFactory.createSettingsNavigation()
                    .startSettings(getActivity(), EnhancedProtectionSettingsFragment.class);
        } else if (safeBrowsingState == SafeBrowsingState.STANDARD_PROTECTION) {
            SettingsNavigationFactory.createSettingsNavigation()
                    .startSettings(getActivity(), StandardProtectionSettingsFragment.class);
        } else {
            assert false : "Should not be reached";
        }
    }

    private ChromeManagedPreferenceDelegate createManagedPreferenceDelegate() {
        return new ChromeManagedPreferenceDelegate(getProfile()) {
            @Override
            public boolean isPreferenceControlledByPolicy(Preference preference) {
                String key = preference.getKey();
                if (PREF_MANAGED_DISCLAIMER_TEXT.equals(key) || PREF_SAFE_BROWSING.equals(key)) {
                    return getSafeBrowsingBridge().isSafeBrowsingManaged();
                } else {
                    assert false : "Should not be reached.";
                }
                return false;
            }
        };
    }

    @Override
    public boolean onPreferenceChange(Preference preference, Object newValue) {
        String key = preference.getKey();
        assert PREF_SAFE_BROWSING.equals(key) : "Unexpected preference key.";
        @SafeBrowsingState int newState = (int) newValue;
        @SafeBrowsingState int currentState = getSafeBrowsingBridge().getSafeBrowsingState();
        if (newState == currentState) {
            return true;
        }
        recordUserActionHistogramForNewStateClicked(newState);
        // If the user selects no protection from another Safe Browsing state, show a confirmation
        // dialog to double check if they want to select no protection.
        if (newState == SafeBrowsingState.NO_SAFE_BROWSING) {
            // The user hasn't confirmed to select no protection, keep the radio button / UI checked
            // state at the currently selected level.
            mSafeBrowsingPreference.setCheckedState(currentState);
            NoProtectionConfirmationDialog.create(
                            getContext(),
                            (didConfirm) -> {
                                recordUserActionHistogramForNoProtectionConfirmation(didConfirm);
                                if (didConfirm) {
                                    // The user has confirmed to select no protection, set Safe
                                    // Browsing pref to no protection, and change the radio button /
                                    // UI checked state to no protection.
                                    getSafeBrowsingBridge()
                                            .setSafeBrowsingState(
                                                    SafeBrowsingState.NO_SAFE_BROWSING);
                                    mSafeBrowsingPreference.setCheckedState(
                                            SafeBrowsingState.NO_SAFE_BROWSING);
                                }
                                // No-ops if the user denies.
                            })
                    .show();
        } else {
            getSafeBrowsingBridge().setSafeBrowsingState(newState);
        }
        return true;
    }

    private void recordUserActionHistogramForNewStateClicked(
            @SafeBrowsingState int safeBrowsingState) {
        switch (safeBrowsingState) {
            case SafeBrowsingState.ENHANCED_PROTECTION:
                recordUserActionHistogram(UserAction.ENHANCED_PROTECTION_CLICKED);
                break;
            case SafeBrowsingState.STANDARD_PROTECTION:
                recordUserActionHistogram(UserAction.STANDARD_PROTECTION_CLICKED);
                break;
            case SafeBrowsingState.NO_SAFE_BROWSING:
                recordUserActionHistogram(UserAction.DISABLE_SAFE_BROWSING_CLICKED);
                break;
            default:
                assert false : "Should not be reached.";
        }
    }

    private void recordUserActionHistogramForStateDetailsClicked(
            @SafeBrowsingState int safeBrowsingState) {
        switch (safeBrowsingState) {
            case SafeBrowsingState.ENHANCED_PROTECTION:
                recordUserActionHistogram(UserAction.ENHANCED_PROTECTION_EXPAND_ARROW_CLICKED);
                break;
            case SafeBrowsingState.STANDARD_PROTECTION:
                recordUserActionHistogram(UserAction.STANDARD_PROTECTION_EXPAND_ARROW_CLICKED);
                break;
            default:
                assert false : "Should not be reached.";
        }
    }

    private void recordUserActionHistogramForNoProtectionConfirmation(boolean didConfirm) {
        if (didConfirm) {
            recordUserActionHistogram(UserAction.DISABLE_SAFE_BROWSING_DIALOG_CONFIRMED);
        } else {
            recordUserActionHistogram(UserAction.DISABLE_SAFE_BROWSING_DIALOG_DENIED);
        }
    }

    private void recordUserActionHistogram(@UserAction int userAction) {
        String metricsSuffix;
        // The metricsSuffix string shouldn't be changed. When adding a new access point, please
        // also update the "SafeBrowsing.Settings.AccessPoint" histogram suffix in the
        // histograms.xml file.
        switch (mAccessPoint) {
            case SettingsAccessPoint.DEFAULT:
                metricsSuffix = "Default";
                break;
            case SettingsAccessPoint.PARENT_SETTINGS:
                metricsSuffix = "ParentSettings";
                break;
            case SettingsAccessPoint.SAFETY_CHECK:
                metricsSuffix = "SafetyCheck";
                break;
            case SettingsAccessPoint.SURFACE_EXPLORER_PROMO_SLINGER:
                metricsSuffix = "SurfaceExplorerPromoSlinger";
                break;
            case SettingsAccessPoint.SECURITY_INTERSTITIAL:
                metricsSuffix = "SecurityInterstitial";
                break;
            case SettingsAccessPoint.TAILORED_SECURITY:
                metricsSuffix = "TailoredSecurity";
                break;
            default:
                assert false : "Should not be reached.";
                metricsSuffix = "";
                break;
        }
        RecordHistogram.recordEnumeratedHistogram(
                "SafeBrowsing.Settings.UserAction." + metricsSuffix,
                userAction,
                UserAction.MAX_VALUE + 1);

        String userActionSuffix;
        switch (userAction) {
            case UserAction.SHOWED:
                userActionSuffix = "ShowedFrom" + metricsSuffix;
                break;
            case UserAction.ENHANCED_PROTECTION_CLICKED:
                userActionSuffix = "EnhancedProtectionClicked";
                break;
            case UserAction.STANDARD_PROTECTION_CLICKED:
                userActionSuffix = "StandardProtectionClicked";
                break;
            case UserAction.DISABLE_SAFE_BROWSING_CLICKED:
                userActionSuffix = "DisableSafeBrowsingClicked";
                break;
            case UserAction.ENHANCED_PROTECTION_EXPAND_ARROW_CLICKED:
                userActionSuffix = "EnhancedProtectionExpandArrowClicked";
                break;
            case UserAction.STANDARD_PROTECTION_EXPAND_ARROW_CLICKED:
                userActionSuffix = "StandardProtectionExpandArrowClicked";
                break;
            case UserAction.DISABLE_SAFE_BROWSING_DIALOG_CONFIRMED:
                userActionSuffix = "DisableSafeBrowsingDialogConfirmed";
                break;
            case UserAction.DISABLE_SAFE_BROWSING_DIALOG_DENIED:
                userActionSuffix = "DisableSafeBrowsingDialogDenied";
                break;
            default:
                assert false : "Should not be reached.";
                userActionSuffix = "";
        }
        RecordUserAction.record("SafeBrowsing.Settings." + userActionSuffix);
    }
}
