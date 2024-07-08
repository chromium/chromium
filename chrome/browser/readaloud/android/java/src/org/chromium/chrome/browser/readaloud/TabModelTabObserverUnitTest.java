// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.readaloud;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.MockTab;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabCreationState;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tab.TabSelectionType;
import org.chromium.chrome.browser.tabmodel.TabModelUtils;
import org.chromium.chrome.test.util.browser.tabmodel.MockTabModel;

/** Tests for the TabModelTabObserver. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class TabModelTabObserverUnitTest {
    private MockTabModel mTabModel;
    @Mock private Profile mProfile;
    private TabModelTabObserver mTabModelTabObserver;

    private Tab mTab;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mTab = new MockTab(1, mProfile);
        mTabModel = new MockTabModel(mProfile, null);
        mTabModel.addTab(0);
        mTabModel.setIndex(0, TabSelectionType.FROM_USER);
        mTabModelTabObserver = new TabModelTabObserver(mTabModel);

        assertTrue(TabModelUtils.getCurrentTab(mTabModel).hasObserver(mTabModelTabObserver));
    }

    @Test
    @SmallTest
    public void testSelectingTabAddsObservers() {
        assertEquals(mTabModel.getCount(), 1);

        mTabModel.addTab(mTab, 1, TabLaunchType.FROM_LINK, TabCreationState.LIVE_IN_FOREGROUND);
        mTabModel.setIndex(1, TabSelectionType.FROM_USER);
        assertEquals(mTabModel.getCount(), 2);
        assertTrue(mTabModel.getTabAt(1).hasObserver(mTabModelTabObserver));
    }

    @Test
    @SmallTest
    public void testDestroyRemovesObservers() {
        assertEquals(mTabModel.getCount(), 1);
        mTabModelTabObserver.destroy();
        assertFalse(mTabModel.getTabAt(0).hasObserver(mTabModelTabObserver));
    }
}
