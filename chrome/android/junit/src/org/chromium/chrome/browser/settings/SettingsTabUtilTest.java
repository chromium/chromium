// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.settings;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabSelectionType;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.ui.native_page.NativePage;

/** Unit tests for {@link SettingsTabUtil}. */
@RunWith(BaseRobolectricTestRunner.class)
public class SettingsTabUtilTest {
    @Test
    public void testIsSettingsTab() {
        assertFalse(SettingsTabUtil.isSettingsTab(null));

        Tab closingTab = mock(Tab.class);
        when(closingTab.isClosing()).thenReturn(true);
        when(closingTab.getNativePage()).thenReturn(mock(SettingsPage.class));
        assertFalse(SettingsTabUtil.isSettingsTab(closingTab));

        Tab destroyedTab = mock(Tab.class);
        when(destroyedTab.isDestroyed()).thenReturn(true);
        when(destroyedTab.getNativePage()).thenReturn(mock(SettingsPage.class));
        assertFalse(SettingsTabUtil.isSettingsTab(destroyedTab));

        Tab incognitoTab = mock(Tab.class);
        when(incognitoTab.isIncognito()).thenReturn(true);
        when(incognitoTab.getNativePage()).thenReturn(mock(SettingsPage.class));
        assertFalse(SettingsTabUtil.isSettingsTab(incognitoTab));

        Tab regularTabNonNative = mock(Tab.class);
        when(regularTabNonNative.getNativePage()).thenReturn(null);
        assertFalse(SettingsTabUtil.isSettingsTab(regularTabNonNative));

        Tab regularTabOtherNative = mock(Tab.class);
        when(regularTabOtherNative.getNativePage()).thenReturn(mock(NativePage.class));
        assertFalse(SettingsTabUtil.isSettingsTab(regularTabOtherNative));

        Tab regularTabSettingsPage = mock(Tab.class);
        when(regularTabSettingsPage.getNativePage()).thenReturn(mock(SettingsPage.class));
        assertTrue(SettingsTabUtil.isSettingsTab(regularTabSettingsPage));
    }

    @Test
    public void testFindSettingsTab() {
        TabModelSelector selector = mock(TabModelSelector.class);
        TabModel regularModel = mock(TabModel.class);
        when(selector.getModel(false)).thenReturn(regularModel);

        Tab tab1 = mock(Tab.class);
        Tab tab2 = mock(Tab.class);
        when(tab2.getNativePage()).thenReturn(mock(SettingsPage.class));

        when(regularModel.getCount()).thenReturn(2);
        when(regularModel.getTabAt(0)).thenReturn(tab1);
        when(regularModel.getTabAt(1)).thenReturn(tab2);

        Tab found = SettingsTabUtil.findSettingsTab(selector);
        assertEquals(tab2, found);

        // Test when no settings tab exists.
        when(regularModel.getCount()).thenReturn(1);
        assertNull(SettingsTabUtil.findSettingsTab(selector));
        assertNull(SettingsTabUtil.findSettingsTab(null));
    }

    @Test
    public void testActivateSettingsTab() {
        TabModelSelector selector = mock(TabModelSelector.class);
        TabModel regularModel = mock(TabModel.class);
        when(selector.getModel(false)).thenReturn(regularModel);
        when(selector.isIncognitoSelected()).thenReturn(true);

        Tab tab = mock(Tab.class);
        when(tab.getNativePage()).thenReturn(mock(SettingsPage.class));
        when(regularModel.indexOf(tab)).thenReturn(3);

        SettingsTabUtil.activateSettingsTab(selector, tab);
        verify(selector).selectModel(false);
        verify(regularModel).setIndex(3, TabSelectionType.FROM_USER);
    }
}
