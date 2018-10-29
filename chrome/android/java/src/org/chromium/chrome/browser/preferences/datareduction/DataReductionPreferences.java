// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.preferences.datareduction;

import android.content.res.Resources;
import android.os.Bundle;
import android.preference.Preference;
import android.preference.Preference.OnPreferenceChangeListener;
import android.preference.PreferenceFragment;
import android.support.graphics.drawable.VectorDrawableCompat;
import android.view.Menu;
import android.view.MenuInflater;
import android.view.MenuItem;

import org.chromium.base.CommandLine;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.help.HelpAndFeedback;
import org.chromium.chrome.browser.infobar.PreviewsLitePageInfoBar;
import org.chromium.chrome.browser.net.spdyproxy.DataReductionProxySettings;
import org.chromium.chrome.browser.preferences.ChromeSwitchPreference;
import org.chromium.chrome.browser.preferences.PreferenceUtils;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.snackbar.DataReductionPromoSnackbarController;
import org.chromium.chrome.browser.util.IntentUtils;

/**
 * Settings fragment that allows the user to configure Data Saver.
 */
public class DataReductionPreferences extends PreferenceFragment {
    public static final String FROM_MAIN_MENU = "FromMainMenu";

    public static final String PREF_DATA_REDUCTION_SWITCH = "data_reduction_switch";

    // This is the same as Chromium data_reduction_proxy::switches::kEnableDataReductionProxy.
    private static final String ENABLE_DATA_REDUCTION_PROXY = "enable-spdy-proxy-auth";

    private boolean mIsEnabled;
    private boolean mWasEnabledAtCreation;
    /** Whether the current Activity is started from the snackbar promo. */
    private boolean mFromPromo;
    private boolean mFromMainMenu;
    private boolean mFromInfobar;

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        PreferenceUtils.addPreferencesFromResource(this, R.xml.data_reduction_preferences);
        getActivity().setTitle(R.string.data_reduction_title);
        boolean isEnabled =
                DataReductionProxySettings.getInstance().isDataReductionProxyEnabled();
        mIsEnabled = !isEnabled;
        mWasEnabledAtCreation = isEnabled;
        updatePreferences(isEnabled);

        setHasOptionsMenu(true);

        if (getActivity() != null) {
            mFromPromo = IntentUtils.safeGetBooleanExtra(getActivity().getIntent(),
                    DataReductionPromoSnackbarController.FROM_PROMO, false);
            mFromMainMenu = IntentUtils.safeGetBooleanExtra(
                    getActivity().getIntent(), FROM_MAIN_MENU, false);
            mFromInfobar = IntentUtils.safeGetBooleanExtra(
                    getActivity().getIntent(), PreviewsLitePageInfoBar.FROM_INFOBAR, false);
        }
    }

    @Override
    public void onDestroy() {
        super.onDestroy();

        if (mWasEnabledAtCreation && !mIsEnabled) {
            // If the user manually disables Data Saver, don't show the infobar promo.
            DataReductionPromoUtils.saveInfoBarPromoDisplayed();
        }

        int statusChange;
        if (mFromPromo) {
            statusChange = mIsEnabled
                    ? DataReductionProxyUma.ACTION_SNACKBAR_LINK_CLICKED
                    : DataReductionProxyUma.ACTION_SNACKBAR_LINK_CLICKED_DISABLED;
        } else if (mFromMainMenu) {
            if (mWasEnabledAtCreation) {
                statusChange = mIsEnabled ? DataReductionProxyUma.ACTION_MAIN_MENU_ON_TO_ON
                                          : DataReductionProxyUma.ACTION_MAIN_MENU_ON_TO_OFF;
            } else {
                statusChange = mIsEnabled ? DataReductionProxyUma.ACTION_MAIN_MENU_OFF_TO_ON
                                          : DataReductionProxyUma.ACTION_MAIN_MENU_OFF_TO_OFF;
            }
        } else if (mFromInfobar) {
            if (mWasEnabledAtCreation) {
                statusChange = mIsEnabled ? DataReductionProxyUma.ACTION_INFOBAR_ON_TO_ON
                                          : DataReductionProxyUma.ACTION_INFOBAR_ON_TO_OFF;
            } else {
                statusChange = mIsEnabled ? DataReductionProxyUma.ACTION_INFOBAR_OFF_TO_ON
                                          : DataReductionProxyUma.ACTION_INFOBAR_OFF_TO_OFF;
            }
        } else if (mWasEnabledAtCreation) {
            statusChange = mIsEnabled
                    ? DataReductionProxyUma.ACTION_ON_TO_ON
                    : DataReductionProxyUma.ACTION_ON_TO_OFF;
        } else {
            statusChange = mIsEnabled
                    ? DataReductionProxyUma.ACTION_OFF_TO_ON
                    : DataReductionProxyUma.ACTION_OFF_TO_OFF;
        }
        DataReductionProxyUma.dataReductionProxyUIAction(statusChange);
    }

    @Override
    public void onCreateOptionsMenu(Menu menu, MenuInflater inflater) {
        menu.clear();
        MenuItem help = menu.add(
                Menu.NONE, R.id.menu_id_targeted_help, Menu.NONE, R.string.menu_help);
        help.setIcon(VectorDrawableCompat.create(
                getResources(), R.drawable.ic_help_and_feedback, getActivity().getTheme()));
    }

    @Override
    public boolean onOptionsItemSelected(MenuItem item) {
        if (item.getItemId() == R.id.menu_id_targeted_help) {
            HelpAndFeedback.getInstance(getActivity())
                    .show(getActivity(), getString(R.string.help_context_data_reduction),
                            Profile.getLastUsedProfile(), null);
            return true;
        }
        return false;
    }

    /**
     * Switches preference screens depending on whether data reduction is enabled/disabled.
     * @param isEnabled Indicates whether data reduction is enabled.
     */
    public void updatePreferences(boolean isEnabled) {
        if (mIsEnabled == isEnabled) return;
        getPreferenceScreen().removeAll();
        createDataReductionSwitch(isEnabled);
        if (isEnabled) {
            PreferenceUtils.addPreferencesFromResource(this, R.xml.data_reduction_preferences);
        } else {
            PreferenceUtils.addPreferencesFromResource(this, R.xml.data_reduction_preferences_off);
        }
        mIsEnabled = isEnabled;
    }

    /**
     * Returns summary string.
     */
    public static String generateSummary(Resources resources) {
        if (DataReductionProxySettings.getInstance().isDataReductionProxyEnabled()) {
            String percent = DataReductionProxySettings.getInstance()
                    .getContentLengthPercentSavings();
            return resources.getString(
                    R.string.data_reduction_menu_item_summary, percent);
        } else {
            return (String) resources.getText(R.string.text_off);
        }
    }

    private void createDataReductionSwitch(boolean isEnabled) {
        final ChromeSwitchPreference dataReductionSwitch =
                new ChromeSwitchPreference(getActivity(), null);
        dataReductionSwitch.setKey(PREF_DATA_REDUCTION_SWITCH);
        dataReductionSwitch.setSummaryOn(R.string.text_on);
        dataReductionSwitch.setSummaryOff(R.string.text_off);
        dataReductionSwitch.setDrawDivider(true);
        dataReductionSwitch.setOnPreferenceChangeListener(new OnPreferenceChangeListener() {
            @Override
            public boolean onPreferenceChange(Preference preference, Object newValue) {
                DataReductionProxySettings.getInstance().setDataReductionProxyEnabled(
                        dataReductionSwitch.getContext(), (boolean) newValue);
                DataReductionPreferences.this.updatePreferences((boolean) newValue);
                return true;
            }
        });
        dataReductionSwitch.setManagedPreferenceDelegate(preference -> {
            return CommandLine.getInstance().hasSwitch(ENABLE_DATA_REDUCTION_PROXY)
                    || DataReductionProxySettings.getInstance().isDataReductionProxyManaged();
        });

        getPreferenceScreen().addPreference(dataReductionSwitch);

        // Note: setting the switch state before the preference is added to the screen results in
        // some odd behavior where the switch state doesn't always match the internal enabled state
        // (e.g. the switch will say "On" when data reduction is really turned off), so
        // .setChecked() should be called after .addPreference()
        dataReductionSwitch.setChecked(isEnabled);
    }
}
