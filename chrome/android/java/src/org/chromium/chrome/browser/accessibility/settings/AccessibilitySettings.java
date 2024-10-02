// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.accessibility.settings;

import android.content.Intent;
import android.os.Bundle;
import android.provider.Settings;

import androidx.preference.Preference;
import androidx.preference.PreferenceFragmentCompat;

import org.chromium.base.ContextUtils;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.image_descriptions.ImageDescriptionsController;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.settings.SettingsNavigationFactory;
import org.chromium.components.browser_ui.accessibility.AccessibilitySettingsDelegate;
import org.chromium.components.browser_ui.accessibility.FontSizePrefs;
import org.chromium.components.browser_ui.accessibility.FontSizePrefs.FontSizePrefsObserver;
import org.chromium.components.browser_ui.accessibility.PageZoomPreference;
import org.chromium.components.browser_ui.accessibility.PageZoomUma;
import org.chromium.components.browser_ui.accessibility.PageZoomUtils;
import org.chromium.components.browser_ui.settings.ChromeSwitchPreference;
import org.chromium.components.browser_ui.settings.EmbeddableSettingsPage;
import org.chromium.components.browser_ui.settings.SettingsNavigation;
import org.chromium.components.browser_ui.settings.SettingsUtils;
import org.chromium.components.browser_ui.site_settings.AllSiteSettings;
import org.chromium.components.browser_ui.site_settings.SingleCategorySettings;
import org.chromium.components.browser_ui.site_settings.SiteSettingsCategory;
import org.chromium.components.omnibox.OmniboxFeatures;
import org.chromium.components.prefs.PrefService;
import org.chromium.content_public.browser.ContentFeatureList;
import org.chromium.content_public.browser.ContentFeatureMap;

/** Fragment to keep track of all the accessibility related preferences. */
public class AccessibilitySettings extends PreferenceFragmentCompat
        implements EmbeddableSettingsPage, Preference.OnPreferenceChangeListener {
    public static final String PREF_TEXT_SCALE = "text_scale";
    public static final String PREF_PAGE_ZOOM_DEFAULT_ZOOM = "page_zoom_default_zoom";
    public static final String PREF_PAGE_ZOOM_INCLUDE_OS_ADJUSTMENT =
            "page_zoom_include_os_adjustment";
    public static final String PREF_PAGE_ZOOM_ALWAYS_SHOW = "page_zoom_always_show";
    public static final String PREF_FORCE_ENABLE_ZOOM = "force_enable_zoom";
    public static final String PREF_READER_FOR_ACCESSIBILITY = "reader_for_accessibility";
    public static final String PREF_CAPTIONS = "captions";
    public static final String PREF_ZOOM_INFO = "zoom_info";
    public static final String PREF_IMAGE_DESCRIPTIONS = "image_descriptions";

    private TextScalePreference mTextScalePref;
    private PageZoomPreference mPageZoomDefaultZoomPref;
    private ChromeSwitchPreference mPageZoomIncludeOSAdjustment;
    private ChromeSwitchPreference mPageZoomAlwaysShowPref;
    private ChromeSwitchPreference mForceEnableZoomPref;
    private ChromeSwitchPreference mJumpStartOmnibox;
    private boolean mRecordFontSizeChangeOnStop;
    private AccessibilitySettingsDelegate mDelegate;
    private double mPageZoomLatestDefaultZoomPrefValue;
    private PrefService mPrefService;

    private FontSizePrefs mFontSizePrefs;
    private FontSizePrefsObserver mFontSizePrefsObserver =
            new FontSizePrefsObserver() {
                @Override
                public void onFontScaleFactorChanged(
                        float fontScaleFactor, float userFontScaleFactor) {
                    mTextScalePref.updateFontScaleFactors(
                            fontScaleFactor, userFontScaleFactor, true);
                }

                @Override
                public void onForceEnableZoomChanged(boolean enabled) {
                    mForceEnableZoomPref.setChecked(enabled);
                }
            };

    private final ObservableSupplierImpl<String> mPageTitle = new ObservableSupplierImpl<>();

    public void setPrefService(PrefService prefService) {
        mPrefService = prefService;
    }

    public void setDelegate(AccessibilitySettingsDelegate delegate) {
        mDelegate = delegate;
        mFontSizePrefs = FontSizePrefs.getInstance(delegate.getBrowserContextHandle());
    }

    @Override
    public void onActivityCreated(Bundle savedInstanceState) {
        super.onActivityCreated(savedInstanceState);

        mPageTitle.set(getString(R.string.prefs_accessibility));
    }

    @Override
    public ObservableSupplier<String> getPageTitle() {
        return mPageTitle;
    }

    @Override
    public void onCreatePreferences(Bundle savedInstanceState, String rootKey) {
        SettingsUtils.addPreferencesFromResource(this, R.xml.accessibility_preferences);

        mTextScalePref = (TextScalePreference) findPreference(PREF_TEXT_SCALE);
        mPageZoomDefaultZoomPref = (PageZoomPreference) findPreference(PREF_PAGE_ZOOM_DEFAULT_ZOOM);
        mPageZoomAlwaysShowPref =
                (ChromeSwitchPreference) findPreference(PREF_PAGE_ZOOM_ALWAYS_SHOW);
        mPageZoomIncludeOSAdjustment =
                (ChromeSwitchPreference) findPreference(PREF_PAGE_ZOOM_INCLUDE_OS_ADJUSTMENT);

        if (ContentFeatureMap.isEnabled(ContentFeatureList.ACCESSIBILITY_PAGE_ZOOM)) {
            mTextScalePref.setVisible(false);
            // Set the initial values for the page zoom settings, and set change listeners.
            mPageZoomDefaultZoomPref.setInitialValue(
                    PageZoomUtils.getDefaultZoomAsSeekBarValue(
                            mDelegate.getBrowserContextHandle()));
            mPageZoomDefaultZoomPref.setOnPreferenceChangeListener(this);
            mPageZoomAlwaysShowPref.setChecked(PageZoomUtils.shouldShowZoomMenuItem());
            mPageZoomAlwaysShowPref.setOnPreferenceChangeListener(this);

            // When Smart Zoom feature is enabled, set the required delegate.
            if (ContentFeatureMap.isEnabled(ContentFeatureList.SMART_ZOOM)) {
                mPageZoomDefaultZoomPref.setTextSizeContrastDelegate(
                        mDelegate.getTextSizeContrastAccessibilityDelegate());
            }
        } else {
            mPageZoomDefaultZoomPref.setVisible(false);
            mPageZoomAlwaysShowPref.setVisible(false);
            mTextScalePref.setOnPreferenceChangeListener(this);
            mTextScalePref.updateFontScaleFactors(
                    mFontSizePrefs.getFontScaleFactor(),
                    mFontSizePrefs.getUserFontScaleFactor(),
                    false);
        }

        mForceEnableZoomPref = (ChromeSwitchPreference) findPreference(PREF_FORCE_ENABLE_ZOOM);
        mForceEnableZoomPref.setOnPreferenceChangeListener(this);
        mForceEnableZoomPref.setChecked(mFontSizePrefs.getForceEnableZoom());

        mJumpStartOmnibox =
                (ChromeSwitchPreference) findPreference(OmniboxFeatures.KEY_JUMP_START_OMNIBOX);
        mJumpStartOmnibox.setOnPreferenceChangeListener(this);
        mJumpStartOmnibox.setChecked(OmniboxFeatures.isJumpStartOmniboxEnabled());
        mJumpStartOmnibox.setVisible(OmniboxFeatures.sJumpStartOmnibox.isEnabled());

        ChromeSwitchPreference readerForAccessibilityPref =
                (ChromeSwitchPreference) findPreference(PREF_READER_FOR_ACCESSIBILITY);
        readerForAccessibilityPref.setChecked(
                mPrefService.getBoolean(Pref.READER_FOR_ACCESSIBILITY));
        readerForAccessibilityPref.setOnPreferenceChangeListener(this);

        Preference captions = findPreference(PREF_CAPTIONS);
        captions.setOnPreferenceClickListener(
                preference -> {
                    Intent intent = new Intent(Settings.ACTION_CAPTIONING_SETTINGS);

                    // Open the activity in a new task because the back button on the caption
                    // settings page navigates to the previous settings page instead of Chrome.
                    intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
                    startActivity(intent);

                    return true;
                });

        Preference zoomInfo = findPreference(PREF_ZOOM_INFO);
        if (ContentFeatureMap.isEnabled(ContentFeatureList.ACCESSIBILITY_PAGE_ZOOM)
                && ContentFeatureMap.isEnabled(
                        ContentFeatureList.ACCESSIBILITY_PAGE_ZOOM_ENHANCEMENTS)) {
            zoomInfo.setVisible(true);
            zoomInfo.setOnPreferenceClickListener(
                    preference -> {
                        Bundle initialArguments = new Bundle();
                        initialArguments.putString(
                                SingleCategorySettings.EXTRA_CATEGORY,
                                SiteSettingsCategory.preferenceKey(SiteSettingsCategory.Type.ZOOM));
                        SettingsNavigation settingsNavigation =
                                SettingsNavigationFactory.createSettingsNavigation();
                        settingsNavigation.startSettings(
                                ContextUtils.getApplicationContext(),
                                AllSiteSettings.class,
                                initialArguments);
                        return true;
                    });

            // When Accessibility Page Zoom v2 is also enabled, show additional controls.
            mPageZoomIncludeOSAdjustment.setVisible(
                    ContentFeatureMap.isEnabled(ContentFeatureList.ACCESSIBILITY_PAGE_ZOOM_V2));
            mPageZoomIncludeOSAdjustment.setOnPreferenceChangeListener(this);
        } else {
            zoomInfo.setVisible(false);
            mPageZoomIncludeOSAdjustment.setVisible(false);
        }

        Preference imageDescriptionsPreference = findPreference(PREF_IMAGE_DESCRIPTIONS);
        imageDescriptionsPreference.setVisible(
                ImageDescriptionsController.getInstance().shouldShowImageDescriptionsMenuItem());
    }

    @Override
    public void onStart() {
        super.onStart();
        mFontSizePrefs.addObserver(mFontSizePrefsObserver);
    }

    @Override
    public void onStop() {
        mFontSizePrefs.removeObserver(mFontSizePrefsObserver);
        if (mRecordFontSizeChangeOnStop) {
            mFontSizePrefs.recordUserFontPrefChange();
            mRecordFontSizeChangeOnStop = false;
        }

        // Ensure that the user has set a default zoom value during this session.
        if (mPageZoomLatestDefaultZoomPrefValue != 0.0) {
            PageZoomUma.logSettingsDefaultZoomLevelChangedHistogram();
            PageZoomUma.logSettingsDefaultZoomLevelValueHistogram(
                    mPageZoomLatestDefaultZoomPrefValue);
        }

        super.onStop();
    }

    @Override
    public boolean onPreferenceChange(Preference preference, Object newValue) {
        if (PREF_TEXT_SCALE.equals(preference.getKey())) {
            mRecordFontSizeChangeOnStop = true;
            mFontSizePrefs.setUserFontScaleFactor((Float) newValue);
        } else if (PREF_FORCE_ENABLE_ZOOM.equals(preference.getKey())) {
            mFontSizePrefs.setForceEnableZoomFromUser((Boolean) newValue);
        } else if (PREF_READER_FOR_ACCESSIBILITY.equals(preference.getKey())) {
            mPrefService.setBoolean(Pref.READER_FOR_ACCESSIBILITY, (Boolean) newValue);
        } else if (PREF_PAGE_ZOOM_DEFAULT_ZOOM.equals(preference.getKey())) {
            mPageZoomLatestDefaultZoomPrefValue =
                    PageZoomUtils.convertSeekBarValueToZoomLevel((Integer) newValue);
            PageZoomUtils.setDefaultZoomBySeekBarValue(
                    mDelegate.getBrowserContextHandle(), (Integer) newValue);
        } else if (PREF_PAGE_ZOOM_ALWAYS_SHOW.equals(preference.getKey())) {
            PageZoomUtils.setShouldAlwaysShowZoomMenuItem((Boolean) newValue);
        } else if (PREF_PAGE_ZOOM_INCLUDE_OS_ADJUSTMENT.equals(preference.getKey())) {
            // TODO(mschillaci): Implement the override behavior for OS level.
        } else if (OmniboxFeatures.KEY_JUMP_START_OMNIBOX.equals(preference.getKey())) {
            OmniboxFeatures.setJumpStartOmniboxEnabled((Boolean) newValue);
        }
        return true;
    }
}
