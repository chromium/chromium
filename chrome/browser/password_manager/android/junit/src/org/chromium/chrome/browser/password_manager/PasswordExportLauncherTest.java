// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_manager;

import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.verify;

import static org.chromium.chrome.browser.password_manager.PasswordExportLauncher.START_PASSWORDS_EXPORT;

import android.content.Context;
import android.os.Bundle;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.settings.SettingsNavigationFactory;
import org.chromium.components.browser_ui.settings.SettingsNavigation;
import org.chromium.components.browser_ui.settings.SettingsNavigation.SettingsFragment;

@RunWith(BaseRobolectricTestRunner.class)
public class PasswordExportLauncherTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Mock private SettingsNavigation mSettingsNavigationMock;
    @Mock private Context mContext;
    @Captor private ArgumentCaptor<Bundle> mBundleCaptor;

    @Before
    public void setUp() {
        SettingsNavigationFactory.setInstanceForTesting(mSettingsNavigationMock);
    }

    @Test
    public void testShowMainSettingsAndStartExport() {
        PasswordExportLauncher.showMainSettingsAndStartExport(mContext);
        verify(mSettingsNavigationMock)
                .createSettingsIntent(
                        eq(mContext), eq(SettingsFragment.MAIN), mBundleCaptor.capture());
        Bundle bundle = mBundleCaptor.getValue();
        assertTrue(bundle.containsKey(START_PASSWORDS_EXPORT));
        assertTrue(bundle.getBoolean(START_PASSWORDS_EXPORT));
    }
}
