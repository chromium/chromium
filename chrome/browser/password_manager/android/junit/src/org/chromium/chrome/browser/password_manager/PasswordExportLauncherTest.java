// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_manager;

import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.verify;

import static org.chromium.chrome.browser.password_manager.PasswordExportLauncher.START_PASSWORDS_EXPORT;

import android.content.Context;
import android.os.Bundle;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.settings.SettingsNavigationFactory;
import org.chromium.components.browser_ui.settings.SettingsNavigation;
import org.chromium.components.browser_ui.settings.SettingsNavigation.SettingsFragment;

@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class PasswordExportLauncherTest {
    @Mock private SettingsNavigation mSettingsNavigationMock;

    @Before
    public void setUp() {
        MockitoAnnotations.openMocks(this);
        SettingsNavigationFactory.setInstanceForTesting(mSettingsNavigationMock);
    }

    @Test
    public void testShowMainSettingsAndStartExport() {
        Context mockContext = mock(Context.class);
        PasswordExportLauncher.showMainSettingsAndStartExport(mockContext);
        ArgumentCaptor<Bundle> bundleArgumentCaptor = ArgumentCaptor.forClass(Bundle.class);
        verify(mSettingsNavigationMock)
                .createSettingsIntent(
                        eq(mockContext), eq(SettingsFragment.MAIN), bundleArgumentCaptor.capture());
        Bundle bundle = bundleArgumentCaptor.getValue();
        assertTrue(bundle.containsKey(START_PASSWORDS_EXPORT));
        assertTrue(bundle.getBoolean(START_PASSWORDS_EXPORT));
    }
}
