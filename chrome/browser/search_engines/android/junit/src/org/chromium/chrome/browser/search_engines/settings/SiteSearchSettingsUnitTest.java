// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.search_engines.settings;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import androidx.fragment.app.FragmentManager;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;

import org.chromium.base.ServiceLoaderUtil;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.search_engines.R;
import org.chromium.chrome.browser.search_engines.TemplateUrlServiceFactory;
import org.chromium.chrome.browser.search_engines.settings.extensions.ExtensionSearchEngineCoordinator;
import org.chromium.chrome.browser.ui.extensions.ExtensionUi;
import org.chromium.chrome.browser.ui.extensions.ExtensionUiBackend;
import org.chromium.components.favicon.LargeIconBridge;
import org.chromium.components.favicon.LargeIconBridgeJni;
import org.chromium.components.search_engines.TemplateUrlService;
import org.chromium.ui.test.util.BlankUiTestActivity;

/** Unit tests for {@link SiteSearchSettings}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class SiteSearchSettingsUnitTest {
    @Rule public MockitoRule mMockitoJUnit = MockitoJUnit.rule();
    private SiteSearchSettings mFragment;
    private BlankUiTestActivity mActivity;
    private @Mock Profile mProfile;
    private @Mock TemplateUrlService mTemplateUrlService;
    private @Mock LargeIconBridge.Natives mLargeIconBridgeJni;
    private @Mock ExtensionUiBackend mExtensionUiBackend;
    private @Mock ExtensionSearchEngineCoordinator mExtensionSearchEngineCoordinator;

    @Before
    public void setUp() {
        TemplateUrlServiceFactory.setInstanceForTesting(mTemplateUrlService);
        LargeIconBridgeJni.setInstanceForTesting(mLargeIconBridgeJni);
        ExtensionUi.setBackendForTesting(mExtensionUiBackend);
        ServiceLoaderUtil.setInstanceForTesting(
                ExtensionSearchEngineCoordinator.class, mExtensionSearchEngineCoordinator);

        mActivity = Robolectric.buildActivity(BlankUiTestActivity.class).setup().get();
        mFragment = new SiteSearchSettings();
        mFragment.setProfile(mProfile);
    }

    private void launchFragment() {
        FragmentManager fragmentManager = mActivity.getSupportFragmentManager();
        fragmentManager.beginTransaction().add(mFragment, null).commitNow();
    }

    @Test
    public void testGetPageTitle() {
        launchFragment();
        assertEquals(
                mActivity.getString(R.string.manage_search_engines_and_site_search),
                mFragment.getPageTitle().get());
    }

    @Test
    public void testPreferencesCreated_ExtensionEnabled() {
        when(mExtensionUiBackend.isEnabled(mProfile)).thenReturn(true);

        launchFragment();

        assertNotNull(mFragment.findPreference("keyboard_shortcut_radio_group"));
        assertNotNull(mFragment.findPreference("custom_search_engine_item_list"));
        assertNotNull(mFragment.findPreference("custom_site_search_item_list"));
        assertNotNull(mFragment.findPreference("inactive_shortcut_list"));
        assertNotNull(mFragment.findPreference("extension_item_list"));
        verify(mExtensionSearchEngineCoordinator).initialize(any(), eq(mProfile), any(), any());
    }

    @Test
    public void testPreferencesCreated_ExtensionDisabled() {
        when(mExtensionUiBackend.isEnabled(mProfile)).thenReturn(false);

        launchFragment();

        assertNotNull(mFragment.findPreference("keyboard_shortcut_radio_group"));
        assertNotNull(mFragment.findPreference("custom_search_engine_item_list"));
        assertNotNull(mFragment.findPreference("custom_site_search_item_list"));
        assertNotNull(mFragment.findPreference("inactive_shortcut_list"));
        // In prod code the coordinator is null since ServiceLoaderUtil cannot find the
        // implementation. Here we only test the pref is not created and the initialize method
        // will not be called.
        assertNull(mFragment.findPreference("extension_item_list"));
        verify(mExtensionSearchEngineCoordinator, never()).initialize(any(), any(), any(), any());
    }
}
