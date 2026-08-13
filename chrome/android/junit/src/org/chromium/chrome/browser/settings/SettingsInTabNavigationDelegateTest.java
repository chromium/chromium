// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.settings;

import static org.junit.Assert.assertEquals;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.verify;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.night_mode.settings.ThemeSettingsFragment;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.content_public.browser.LoadUrlParams;

/** Unit tests for {@link SettingsInTabNavigationDelegate}. */
@RunWith(BaseRobolectricTestRunner.class)
public class SettingsInTabNavigationDelegateTest {

    @Test
    public void testSettingsInTabNavigationDelegate_LoadsCanonicalUrl() {
        Tab mockTab = mock(Tab.class);
        SettingsInTabNavigationDelegate delegate = new SettingsInTabNavigationDelegate(mockTab);

        // Verify that startSettings() invokes tab.loadUrl() with the canonical
        // sublevel chrome://settings URL.
        delegate.startSettings(null, ThemeSettingsFragment.class);

        ArgumentCaptor<LoadUrlParams> captor = ArgumentCaptor.forClass(LoadUrlParams.class);
        verify(mockTab).loadUrl(captor.capture());
        assertEquals("chrome://settings/theme", captor.getValue().getUrl());
    }
}
