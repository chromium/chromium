// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.datareduction.settings;

import android.content.res.Configuration;
import android.content.res.Resources;
import android.os.Bundle;
import android.view.Menu;
import android.view.MenuInflater;
import android.view.MenuItem;

import androidx.preference.Preference;
import androidx.preference.PreferenceFragmentCompat;
import androidx.vectordrawable.graphics.drawable.VectorDrawableCompat;

import org.chromium.base.CommandLine;
import org.chromium.base.IntentUtils;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.datareduction.DataReductionPromoUtils;
import org.chromium.chrome.browser.datareduction.DataReductionProxyUma;
import org.chromium.chrome.browser.feedback.HelpAndFeedbackLauncherImpl;
import org.chromium.chrome.browser.infobar.PreviewsLitePageInfoBar;
import org.chromium.chrome.browser.net.spdyproxy.DataReductionProxySettings;
import org.chromium.chrome.browser.net.spdyproxy.DataReductionProxySettings.ContentLengths;
import org.chromium.chrome.browser.previews.HttpsImageCompressionUtils;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.settings.ChromeManagedPreferenceDelegate;
import org.chromium.components.browser_ui.settings.ChromeSwitchPreference;
import org.chromium.components.browser_ui.settings.SettingsUtils;
import org.chromium.components.browser_ui.util.ConversionUtils;

import java.text.NumberFormat;
import java.util.Locale;

/**
 * Settings fragment that allows the user to configure Data Saver.
 */
public class DataReductionPreferenceFragment extends PreferenceFragmentCompat {
    public static final String FROM_MAIN_MENU = "FromMainMenu";

    public static final String PREF_DATA_REDUCTION_SWITCH = "data_reduction_switch";
    public static final String PREF_LEARN_MORE_KEY = "data_reduction_learn_more";

    // This is the same as Chromium data_reduction_proxy::switches::kEnableDataReductionProxy.
    private static final String ENABLE_DATA_REDUCTION_PROXY = "enable-spdy-proxy-auth";

    private boolean mIsEnabled;
    private boolean mWasEnabledAtCreation;
    private boolean mFromMainMenu;
    private boolean mFromInfobar;
    private boolean mFromLiteModeHttpsImageCompressionInfoBar;

    @Override
    public void onCreatePreferences(Bundle savedInstanceState, String rootKey) {
        SettingsUtils.addPreferencesFromResource(this, R.xml.data_reduction_preferences);
        getActivity().setTitle(R.string.data_reduction_title_lite_mode);
        boolean isEnabled = DataReductionProxySettings.getInstance().isDataReductionProxyEnabled();
        mIsEnabled = !isEnabled;
        mWasEnabledAtCreation = isEnabled;
        updatePreferences(isEnabled);

        setHasOptionsMenu(true);

        mFromMainMenu = IntentUtils.safeGetBoolean(getArguments(), FROM_MAIN_MENU, false);
        mFromInfobar = IntentUtils.safeGetBoolean(
                getArguments(), PreviewsLitePageInfoBar.FROM_INFOBAR, false);
        mFromLiteModeHttpsImageCompressionInfoBar = IntentUtils.safeGetBoolean(getArguments(),
                HttpsImageCompressionUtils.FROM_LITE_MODE_HTTPS_IMAGE_COMPRESSION_INFOBAR, false);
    }

    @Override
    public void onDestroy() {
        super.onDestroy();

        if (mWasEnabledAtCreation && !mIsEnabled) {
            // If the user manually disables Data Saver, don't show the infobar promo.
            DataReductionPromoUtils.saveInfoBarPromoDisplayed();
        }

        int statusChange;
        if (mFromMainMenu) {
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
        } else if (mFromLiteModeHttpsImageCompressionInfoBar) {
            if (mWasEnabledAtCreation) {
                statusChange = mIsEnabled
                        ? DataReductionProxyUma.ACTION_HTTPS_IMAGE_COMPRESSION_INFOBAR_ON_TO_ON
                        : DataReductionProxyUma.ACTION_HTTPS_IMAGE_COMPRESSION_INFOBAR_ON_TO_OFF;
            } else {
                statusChange = mIsEnabled
                        ? DataReductionProxyUma.ACTION_HTTPS_IMAGE_COMPRESSION_INFOBAR_OFF_TO_ON
                        : DataReductionProxyUma.ACTION_HTTPS_IMAGE_COMPRESSION_INFOBAR_OFF_TO_OFF;
            }
        } else if (mWasEnabledAtCreation) {
            statusChange = mIsEnabled ? DataReductionProxyUma.ACTION_ON_TO_ON
                                      : DataReductionProxyUma.ACTION_ON_TO_OFF;
        } else {
            statusChange = mIsEnabled ? DataReductionProxyUma.ACTION_OFF_TO_ON
                                      : DataReductionProxyUma.ACTION_OFF_TO_OFF;
        }
        DataReductionProxyUma.dataReductionProxyUIAction(statusChange);
    }

    @Override
    public void onCreateOptionsMenu(Menu menu, MenuInflater inflater) {
        menu.clear();
        MenuItem help =
                menu.add(Menu.NONE, R.id.menu_id_targeted_help, Menu.NONE, R.string.menu_help);
        help.setIcon(VectorDrawableCompat.create(
                getResources(), R.drawable.ic_help_and_feedback, getActivity().getTheme()));
    }

    @Override
    public boolean onOptionsItemSelected(MenuItem item) {
        if (item.getItemId() == R.id.menu_id_targeted_help) {
            HelpAndFeedbackLauncherImpl.getInstance().show(getActivity(),
                    getString(R.string.help_context_data_reduction),
                    Profile.getLastUsedRegularProfile(), null);
            return true;
        }
        return false;
    }

    @Override
    public void onConfigurationChanged(Configuration newConfig) {
        // Force rebinding of preferences on orientation change, otherwise the usage chart will not
        // be correctly redrawn: https://crbug.com/994668.
        getListView().getAdapter().notifyDataSetChanged();

        super.onConfigurationChanged(newConfig);
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
            SettingsUtils.addPreferencesFromResource(this, R.xml.data_reduction_preferences);
        } else {
            SettingsUtils.addPreferencesFromResource(
                    this, R.xml.data_reduction_preferences_off_lite_mode);

            // Configure "Learn more" link.
            Preference learnMorePreference = findPreference(PREF_LEARN_MORE_KEY);
            learnMorePreference.setOnPreferenceClickListener(preference -> {
                HelpAndFeedbackLauncherImpl.getInstance().show(getActivity(),
                        getString(R.string.help_context_data_reduction),
                        Profile.getLastUsedRegularProfile(), null);
                return true;
            });
        }
        mIsEnabled = isEnabled;
    }

    /**
     * Returns summary string.
     */
    public static String generateSummary(Resources resources) {
        if (DataReductionProxySettings.getInstance().isDataReductionProxyEnabled()) {
            ContentLengths length = DataReductionProxySettings.getInstance().getContentLengths();

            // If received is less than show chart threshold than don't show summary.
            if (ConversionUtils.bytesToKilobytes(length.getReceived())
                    < DataReductionProxySettings.DATA_REDUCTION_SHOW_CHART_KB_THRESHOLD) {
                return "";
            }

            String percent = generatePercentSavings(length);
            return resources.getString(
                    R.string.data_reduction_menu_item_summary_lite_mode, percent);
        } else {
            return (String) resources.getText(R.string.text_off);
        }
    }

    /**
     * Returns formatted percent savings as string from ContentLengths
     */
    private static String generatePercentSavings(ContentLengths length) {
        double savings = 0;
        if (length.getOriginal() > 0L && length.getOriginal() > length.getReceived()) {
            savings = (length.getOriginal() - length.getReceived()) / (double) length.getOriginal();
        }
        NumberFormat percentageFormatter = NumberFormat.getPercentInstance(Locale.getDefault());
        return percentageFormatter.format(savings);
    }

    private void createDataReductionSwitch(boolean isEnabled) {
        final ChromeSwitchPreference dataReductionSwitch =
                new ChromeSwitchPreference(getPreferenceManager().getContext(), null);
        dataReductionSwitch.setKey(PREF_DATA_REDUCTION_SWITCH);
        dataReductionSwitch.setSummaryOn(R.string.text_on);
        dataReductionSwitch.setSummaryOff(R.string.text_off);
        dataReductionSwitch.setOnPreferenceChangeListener((preference, newValue) -> {
            DataReductionProxySettings.getInstance().setDataReductionProxyEnabled(
                    dataReductionSwitch.getContext(), (boolean) newValue);
            DataReductionPreferenceFragment.this.updatePreferences((boolean) newValue);
            return true;
        });
        dataReductionSwitch.setManagedPreferenceDelegate(
                (ChromeManagedPreferenceDelegate) preference -> {
                    return CommandLine.getInstance().hasSwitch(ENABLE_DATA_REDUCTION_PROXY)
                            || DataReductionProxySettings.getInstance()
                                       .isDataReductionProxyManaged();
                });

        getPreferenceScreen().addPreference(dataReductionSwitch);

        // Note: setting the switch state before the preference is added to the screen results in
        // some odd behavior where the switch state doesn't always match the internal enabled state
        // (e.g. the switch will say "On" when data reduction is really turned off), so
        // .setChecked() should be called after .addPreference()
        dataReductionSwitch.setChecked(isEnabled);
    }
}
