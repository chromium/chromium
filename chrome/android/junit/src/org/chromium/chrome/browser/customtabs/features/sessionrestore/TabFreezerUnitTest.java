// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs.features.sessionrestore;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.verify;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.mockito.quality.Strictness;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.tab.Tab;

/**
 * Unit tests for {@link TabFreezer}.
 */
@RunWith(BaseRobolectricTestRunner.class)
public class TabFreezerUnitTest {
    @Rule
    public MockitoRule mMockitoRule = MockitoJUnit.rule().strictness(Strictness.STRICT_STUBS);

    private TabFreezer mTabFreezer;

    @Mock
    private Tab mTab;

    @Before
    public void setup() {
        mTabFreezer = new TabFreezer();
    }

    @Test
    public void emptyFreeze() {
        assertFalse("No tab exists in empty freezer.", mTabFreezer.hasTab());
        assertEquals("No tab to be cleared.", Tab.INVALID_TAB_ID, mTabFreezer.clear());
        assertNull("No tab to unfreeze.", mTabFreezer.unfreeze());
    }

    @Test
    public void freezeTabThenRestore() {
        mTabFreezer.freeze(mTab);
        verify(mTab).hide(anyInt());
        assertTrue("Freezer should have tab.", mTabFreezer.hasTab());

        Tab unfrozenTab = mTabFreezer.unfreeze();
        assertEquals("Unfrozen tab changed.", mTab, unfrozenTab);
        assertFalse("No tab exists after unfreeze.", mTabFreezer.hasTab());
    }

    @Test
    public void freezeTabThenClear() {
        final int testTabId = 6173; // Arbitrary number.
        doReturn(testTabId).when(mTab).getId();
        mTabFreezer.freeze(mTab);
        verify(mTab).hide(anyInt());
        assertTrue("Freezer should have tab.", mTabFreezer.hasTab());

        int clearedTabId = mTabFreezer.clear();
        verify(mTab).getId();
        verify(mTab).destroy();
        assertEquals("Removed tabId does not match.", testTabId, clearedTabId);
        assertFalse("No tab exists in after clear.", mTabFreezer.hasTab());
    }
}
