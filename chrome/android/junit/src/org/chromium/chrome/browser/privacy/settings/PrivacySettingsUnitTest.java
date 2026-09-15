// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.privacy.settings;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.argThat;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.verify;

import android.content.Context;

import androidx.test.core.app.ApplicationProvider;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.settings.SettingsNavigationFactory;
import org.chromium.components.browser_ui.settings.SettingsNavigation;
import org.chromium.components.browser_ui.settings.search.SettingsIndexData;
import org.chromium.components.browser_ui.site_settings.SingleCategorySettings;

/** Unit tests for {@link PrivacySettings}. */
@RunWith(BaseRobolectricTestRunner.class)
public class PrivacySettingsUnitTest {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private SettingsNavigation mSettingsNavigation;
    @Mock private SettingsIndexData mSearchIndexDataMock;
    @Mock private Profile mProfile;

    private Context mContext;

    @Before
    public void setUp() {
        mContext = ApplicationProvider.getApplicationContext();
        SettingsNavigationFactory.setInstanceForTesting(mSettingsNavigation);
    }

    @Test
    public void testOnJavascriptOptimizerLinkClicked() {
        PrivacySettings.onJavascriptOptimizerLinkClicked(mContext);

        verify(mSettingsNavigation)
                .startSettings(
                        any(),
                        eq(SingleCategorySettings.class),
                        argThat(
                                fragmentArgs -> {
                                    String category =
                                            fragmentArgs.getString(
                                                    SingleCategorySettings.EXTRA_CATEGORY);
                                    return "javascript_optimizer".equals(category);
                                }),
                        eq(true));
    }

    @Test
    @DisableFeatures({
        ChromeFeatureList.UNIVERSAL_OPT_OUT_SETTINGS,
        ChromeFeatureList.HTTPS_FIRST_BALANCED_MODE
    })
    public void testSearchableIndex_UniversalOptOutSettings_RemovedWhenFeatureDisabled() {
        var indexProvider = PrivacySettings.SEARCH_INDEX_DATA_PROVIDER;
        indexProvider.updateDynamicPreferences(mContext, mSearchIndexDataMock, mProfile);

        verify(mSearchIndexDataMock)
                .removeEntry(indexProvider.getUniqueId(PrivacySettings.PREF_UNIVERSAL_OPT_OUT));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.HTTPS_FIRST_BALANCED_MODE)
    @DisableFeatures(ChromeFeatureList.UNIVERSAL_OPT_OUT_SETTINGS)
    public void testSearchableIndex_HttpsFirstBalancedModeEnabled() {
        var indexProvider = PrivacySettings.SEARCH_INDEX_DATA_PROVIDER;
        indexProvider.updateDynamicPreferences(mContext, mSearchIndexDataMock, mProfile);

        verify(mSearchIndexDataMock)
                .removeEntry(indexProvider.getUniqueId("https_first_mode_legacy"));
    }
}
