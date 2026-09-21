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

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabSelectionType;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.ui.native_page.NativePage;

/** Unit tests for {@link SettingsTabUtil}. */
@RunWith(BaseRobolectricTestRunner.class)
public class SettingsTabUtilTest {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Mock private Tab mClosingTab;
    @Mock private Tab mDestroyedTab;
    @Mock private Tab mIncognitoTab;
    @Mock private Tab mRegularTabNonNative;
    @Mock private Tab mRegularTabOtherNative;
    @Mock private Tab mRegularTabSettingsPage;
    @Mock private TabModelSelector mTabModelSelector;
    @Mock private TabModel mTabModel;
    @Mock private Tab mTab1;
    @Mock private Tab mTab2;
    @Mock private Tab mTab;
    @Mock private NativePage mNativePage;
    @Mock private SettingsPage mSettingsPage;

    @Test
    public void testIsSettingsTab() {
        assertFalse(SettingsTabUtil.isSettingsTab(null));

        when(mClosingTab.isClosing()).thenReturn(true);
        when(mClosingTab.getNativePage()).thenReturn(mock(SettingsPage.class));
        assertFalse(SettingsTabUtil.isSettingsTab(mClosingTab));

        when(mDestroyedTab.isDestroyed()).thenReturn(true);
        when(mDestroyedTab.getNativePage()).thenReturn(mock(SettingsPage.class));
        assertFalse(SettingsTabUtil.isSettingsTab(mDestroyedTab));

        when(mIncognitoTab.isIncognito()).thenReturn(true);
        when(mIncognitoTab.getNativePage()).thenReturn(mock(SettingsPage.class));
        assertFalse(SettingsTabUtil.isSettingsTab(mIncognitoTab));

        when(mRegularTabNonNative.getNativePage()).thenReturn(null);
        assertFalse(SettingsTabUtil.isSettingsTab(mRegularTabNonNative));

        when(mRegularTabOtherNative.getNativePage()).thenReturn(mNativePage);
        assertFalse(SettingsTabUtil.isSettingsTab(mRegularTabOtherNative));

        when(mRegularTabSettingsPage.getNativePage()).thenReturn(mock(SettingsPage.class));
        assertTrue(SettingsTabUtil.isSettingsTab(mRegularTabSettingsPage));
    }

    @Test
    public void testFindSettingsTab() {
        when(mTabModelSelector.getModel(false)).thenReturn(mTabModel);

        when(mTab2.getNativePage()).thenReturn(mSettingsPage);

        when(mTabModel.getCount()).thenReturn(2);
        when(mTabModel.getTabAt(0)).thenReturn(mTab1);
        when(mTabModel.getTabAt(1)).thenReturn(mTab2);

        Tab found = SettingsTabUtil.findSettingsTab(mTabModelSelector);
        assertEquals(mTab2, found);

        // Test when no settings tab exists.
        when(mTabModel.getCount()).thenReturn(1);
        assertNull(SettingsTabUtil.findSettingsTab(mTabModelSelector));
        assertNull(SettingsTabUtil.findSettingsTab(null));
    }

    @Test
    public void testActivateSettingsTab() {
        when(mTabModelSelector.getModel(false)).thenReturn(mTabModel);
        when(mTabModelSelector.isIncognitoSelected()).thenReturn(true);

        when(mTab.getNativePage()).thenReturn(mSettingsPage);
        when(mTabModel.indexOf(mTab)).thenReturn(3);

        SettingsTabUtil.activateSettingsTab(mTabModelSelector, mTab);
        verify(mTabModelSelector).selectModel(false);
        verify(mTabModel).setIndex(3, TabSelectionType.FROM_USER);
    }
}
