// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.search_engines.settings.extensions;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doAnswer;
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
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.search_engines.ExtensionControlHandler;
import org.chromium.chrome.browser.search_engines.TemplateUrlServiceFactory;
import org.chromium.chrome.browser.search_engines.settings.common.SearchEngineListPreference;
import org.chromium.components.browser_ui.settings.SettingsCustomTabLauncher;
import org.chromium.components.favicon.LargeIconBridgeJni;
import org.chromium.components.search_engines.SearchEngineSettingsDataProvider;
import org.chromium.components.search_engines.TemplateUrlService;

/** Unit tests for {@link ExtensionSearchEngineCoordinatorImpl}. */
@RunWith(BaseRobolectricTestRunner.class)
public class ExtensionSearchEngineCoordinatorUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    private Context mContext;
    @Mock private Profile mProfile;
    @Mock private SearchEngineListPreference mPreference;
    @Mock private SettingsCustomTabLauncher mSettingsCustomTabLauncher;
    @Mock private TemplateUrlService mTemplateUrlService;
    @Mock private LargeIconBridgeJni mLargeIconBridgeJni;
    @Mock private ExtensionControlHandler mMockExtensionControlHandler;
    @Mock private SearchEngineSettingsDataProvider mSettingsDataProvider;

    private ExtensionSearchEngineCoordinatorImpl mCoordinator;

    @Before
    public void setUp() {
        mContext = ApplicationProvider.getApplicationContext();
        TemplateUrlServiceFactory.setInstanceForTesting(mTemplateUrlService);
        LargeIconBridgeJni.setInstanceForTesting(mLargeIconBridgeJni);
        ExtensionControlHandler.setFactoryForTesting(() -> mMockExtensionControlHandler);

        doAnswer(
                        invocation -> {
                            Runnable runnable = invocation.getArgument(0);
                            runnable.run();
                            return null;
                        })
                .when(mTemplateUrlService)
                .runWhenLoaded(any());

        mCoordinator = new ExtensionSearchEngineCoordinatorImpl();
    }

    @Test
    public void testInitialization() {
        mCoordinator.initialize(
                mContext, mProfile, mPreference, mSettingsCustomTabLauncher, mSettingsDataProvider);
        verify(mPreference).setAdapter(any(ExtensionSearchEngineAdapter.class));
    }

    @Test
    public void testDestroy() {
        mCoordinator.initialize(
                mContext, mProfile, mPreference, mSettingsCustomTabLauncher, mSettingsDataProvider);
        mCoordinator.destroy();

        verify(mPreference).setAdapter(eq(null));
        verify(mMockExtensionControlHandler).destroy();
    }
}
