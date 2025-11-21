// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.homepage.settings;

import android.os.Bundle;

import androidx.annotation.VisibleForTesting;
import androidx.preference.Preference;

import org.chromium.base.metrics.RecordUserAction;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.homepage.HomepageManager;
import org.chromium.chrome.browser.homepage.HomepagePolicyManager;
import org.chromium.chrome.browser.homepage.settings.RadioButtonGroupHomepagePreference.HomepageOption;
import org.chromium.chrome.browser.homepage.settings.RadioButtonGroupHomepagePreference.PreferenceValues;
import org.chromium.chrome.browser.settings.ChromeBaseSettingsFragment;
import org.chromium.chrome.browser.settings.ChromeManagedPreferenceDelegate;
import org.chromium.chrome.browser.settings.search.BaseSearchIndexProvider;
import org.chromium.components.browser_ui.settings.ChromeSwitchPreference;
import org.chromium.components.browser_ui.settings.SettingsUtils;
import org.chromium.components.embedder_support.util.UrlUtilities;
import org.chromium.components.url_formatter.UrlFormatter;
import org.chromium.url.GURL;

/** Fragment that allows the user to configure homepage related preferences. */
@NullMarked
public class HomepageSettings extends ChromeBaseSettingsFragment {
    @VisibleForTesting public static final String PREF_HOMEPAGE_SWITCH = "homepage_switch";

    @VisibleForTesting
    public static final String PREF_HOMEPAGE_RADIO_GROUP = "homepage_radio_group";

    private HomepageManager mHomepageManager;
    private RadioButtonGroupHomepagePreference mRadioButtons;
    private final ObservableSupplierImpl<String> mPageTitle = new ObservableSupplierImpl<>();

    @Override
    public void onCreatePreferences(@Nullable Bundle savedInstanceState, @Nullable String rootKey) {
        mHomepageManager = HomepageManager.getInstance();

        mPageTitle.set(getString(R.string.options_homepage_title));
        SettingsUtils.addPreferencesFromResource(this, R.xml.homepage_preferences);

        // Set up preferences inside the activity.
        ChromeSwitchPreference homepageSwitch =
                (ChromeSwitchPreference) findPreference(PREF_HOMEPAGE_SWITCH);
        homepageSwitch.setManagedPreferenceDelegate(
                new ChromeManagedPreferenceDelegate(getProfile()) {
                    @Override
                    public boolean isPreferenceControlledByPolicy(Preference preference) {
                        return HomepagePolicyManager.isHomepageLocationManaged()
                                || HomepagePolicyManager.isShowHomeButtonManaged()
                                || HomepagePolicyManager.isHomepageNewTabPageManaged();
                    }

                    @Override
                    public @Nullable Boolean isPreferenceRecommendation(Preference preference) {
                        if (!HomepagePolicyManager.isShowHomeButtonRecommended()) return null;
                        return HomepagePolicyManager.isFollowingHomepageButtonRecommendation();
                    }
                });

        mRadioButtons =
                (RadioButtonGroupHomepagePreference) findPreference(PREF_HOMEPAGE_RADIO_GROUP);
        mRadioButtons.setManagedPreferenceDelegate(
                new ChromeManagedPreferenceDelegate(getProfile()) {
                    @Override
                    public boolean isPreferenceControlledByPolicy(Preference preference) {
                        // If the mRadioButtons are controlled by policy, the associated managed
                        // message is displayed under the switch instead of beneath them.
                        return false;
                    }

                    @Override
                    public @Nullable Boolean isPreferenceRecommendation(Preference preference) {
                        // If the switch is managed due to the homepage location or homepageIsNTP
                        // policies, then there cannot be a homepage selection recommendation.
                        if (HomepagePolicyManager.isHomepageLocationManaged()
                                || HomepagePolicyManager.isHomepageNewTabPageManaged()
                                || (HomepagePolicyManager.isShowHomeButtonManaged()
                                        && !HomepagePolicyManager.getShowHomeButtonValue())
                                || !HomepagePolicyManager.isHomepageSelectionRecommended()) {
                            return null;
                        }
                        return HomepagePolicyManager.isFollowingHomepageSelectionRecommendation();
                    }
                });

        // Set up listeners and update the page.
        boolean isHomepageEnabled = mHomepageManager.isHomepageEnabled();
        homepageSwitch.setChecked(isHomepageEnabled);
        homepageSwitch.setOnPreferenceChangeListener(
                (preference, newValue) -> {
                    onSwitchPreferenceChange((boolean) newValue);
                    return true;
                });
        mRadioButtons.setOnHomepagePreferenceChangeListener(this::onRadioButtonGroupChanged);
        mRadioButtons.setupPreferenceValues(createPreferenceValuesForRadioGroup());

        RecordUserAction.record("Settings.Homepage.Opened");
    }

    @Override
    public ObservableSupplier<String> getPageTitle() {
        return mPageTitle;
    }

    @Override
    public void onStart() {
        super.onStart();
        // If view created, update the state for pref values or policy state changes.
        if (mRadioButtons != null) {
            mRadioButtons.setupPreferenceValues(createPreferenceValuesForRadioGroup());
        }
    }

    /**
     * Handle the preference changes when the homepage switch is toggled.
     *
     * @param isChecked Whether switch is turned on.
     */
    private void onSwitchPreferenceChange(boolean isChecked) {
        mHomepageManager.setPrefHomepageEnabled(isChecked);
        mRadioButtons.setupPreferenceValues(createPreferenceValuesForRadioGroup());
        mRadioButtons.scheduleManagedViewUpdate();
    }

    /**
     * Handles user changes to the homepage selection in the radio button group. Updates relevant
     * state and managed UI.
     *
     * @param newValues The {@link PreferenceValues} object containing the new state of the radio
     *     button group, including the selected option and any custom URI.
     */
    private void onRadioButtonGroupChanged(PreferenceValues newValues) {
        updateHomepageFromRadioGroupPreference(newValues);
        mRadioButtons.scheduleManagedViewUpdate();
    }

    /**
     * Will be called when the status of {@link #mRadioButtons} is changed.
     * TODO(crbug.com/40148533): Record changes whenever user changes settings rather than homepage
     * settings is stopped.
     *
     * @param newValue The {@link PreferenceValues} that the {@link
     *     RadioButtonGroupHomepagePreference} is holding.
     */
    private void updateHomepageFromRadioGroupPreference(PreferenceValues newValue) {
        // When the preference is changed by code during initialization due to policy, ignore the
        // changes of the preference.
        if (HomepagePolicyManager.isHomepageLocationManaged()) {
            return;
        }

        boolean setToUseNtp = newValue.getCheckedOption() == HomepageOption.ENTRY_CHROME_NTP;
        GURL newHomepage = UrlFormatter.fixupUrl(newValue.getCustomURI());
        if (!newHomepage.isValid()) {
            newHomepage = GURL.emptyGURL();
        }
        boolean useDefaultUri =
                mHomepageManager
                        .getDefaultHomepageGurl(getProfile().isOffTheRecord())
                        .equals(newHomepage);

        mHomepageManager.setHomepageSelection(setToUseNtp, useDefaultUri, newHomepage);
    }

    /**
     * @return The user customized homepage setting.
     */
    private GURL getHomepageForEditText() {
        if (HomepagePolicyManager.isHomepageNewTabPageEnabled()) {
            return GURL.emptyGURL();
        }

        if (HomepagePolicyManager.isHomepageLocationManaged()) {
            return HomepagePolicyManager.getHomepageUrl();
        }

        GURL defaultGurl = mHomepageManager.getDefaultHomepageGurl(getProfile().isOffTheRecord());
        GURL customGurl = mHomepageManager.getPrefHomepageCustomGurl();
        if (mHomepageManager.getPrefHomepageUseDefaultUri()) {
            return UrlUtilities.isNtpUrl(defaultGurl) ? GURL.emptyGURL() : defaultGurl;
        }

        if (customGurl.isEmpty() && !UrlUtilities.isNtpUrl(defaultGurl)) {
            return defaultGurl;
        }

        return customGurl;
    }

    private PreferenceValues createPreferenceValuesForRadioGroup() {
        boolean isHomepageLocationManaged = HomepagePolicyManager.isHomepageLocationManaged();
        boolean isHomepageEnabled = mHomepageManager.isHomepageEnabled();
        // The HomepageIsNTP policy overrides radio buttons behavior.
        boolean isNtpPolicyManaged = HomepagePolicyManager.isHomepageNewTabPageManaged();
        if (isNtpPolicyManaged) {
            boolean homepageIsNtp = HomepagePolicyManager.getHomepageNewTabPageValue();
            @HomepageOption
            int optionChecked =
                    homepageIsNtp
                            ? HomepageOption.ENTRY_CHROME_NTP
                            : HomepageOption.ENTRY_CUSTOM_URI;
            // Homepage location is enabled if HomepageIsNTP is false, HomepageLocation policy
            // is unmanaged, and homepage is not otherwise disabled.
            boolean enabled = !homepageIsNtp && !isHomepageLocationManaged && isHomepageEnabled;
            return new PreferenceValues(
                    /* checkedOption= */ optionChecked,
                    /* customizedText= */ getHomepageForEditText().getSpec(),
                    /* isEnabled= */ enabled,
                    /* isNtpButtonVisible= */ homepageIsNtp,
                    /* isCustomizedOptionVisible= */ !homepageIsNtp);
        }

        // Check if the NTP button should be checked.
        // Note it is not always checked when homepage is NTP. When user customized homepage is NTP
        // URL, we don't check Chrome's Homepage radio button.
        boolean shouldCheckNtp;
        if (isHomepageLocationManaged) {
            shouldCheckNtp = UrlUtilities.isNtpUrl(HomepagePolicyManager.getHomepageUrl());
        } else {
            shouldCheckNtp =
                    mHomepageManager.getPrefHomepageUseChromeNtp()
                            || (mHomepageManager.getPrefHomepageUseDefaultUri()
                                    && UrlUtilities.isNtpUrl(
                                            mHomepageManager.getDefaultHomepageGurl(
                                                    getProfile().isOffTheRecord())));
        }

        @HomepageOption
        int checkedOption =
                shouldCheckNtp ? HomepageOption.ENTRY_CHROME_NTP : HomepageOption.ENTRY_CUSTOM_URI;

        boolean isEnabled = !isHomepageLocationManaged && isHomepageEnabled;

        // NTP should be visible when policy is not enforced or the option is checked.
        boolean isNtpOptionVisible = !isHomepageLocationManaged || shouldCheckNtp;

        // Customized option should be visible when policy is not enforced or the option is checked.
        boolean isCustomizedOptionVisible = !isHomepageLocationManaged || !shouldCheckNtp;

        return new PreferenceValues(
                checkedOption,
                getHomepageForEditText().getSpec(),
                isEnabled,
                isNtpOptionVisible,
                isCustomizedOptionVisible);
    }

    ChromeSwitchPreference getHomepageSwitchForTesting() {
        return (ChromeSwitchPreference) findPreference(PREF_HOMEPAGE_SWITCH);
    }

    RadioButtonGroupHomepagePreference getHomepageRadioGroupForTesting() {
        return mRadioButtons;
    }

    @Override
    public @AnimationType int getAnimationType() {
        return AnimationType.PROPERTY;
    }

    @Override
    public @Nullable String getMainMenuKey() {
        return "homepage";
    }

    public static final BaseSearchIndexProvider SEARCH_INDEX_DATA_PROVIDER =
            new BaseSearchIndexProvider(
                    HomepageSettings.class.getName(), R.xml.homepage_preferences);
}
