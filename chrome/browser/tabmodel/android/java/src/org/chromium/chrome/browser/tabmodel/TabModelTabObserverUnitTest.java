// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

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
import org.chromium.chrome.browser.tab.TabSelectionType;
import org.chromium.chrome.test.util.browser.tabmodel.MockTabModel;

/**
 * Tests for the TabModelTabObserver.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class TabModelTabObserverUnitTest {
    private MockTabModel mTabModel;
    @Mock
    private Profile mProfile;
    private TabModelTabObserver mTabModelTabObserver;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mTabModel = new MockTabModel(false, null);
        mTabModel.addTab(0);
        mTabModel.setIndex(0, TabSelectionType.FROM_USER, false);
        mTabModelTabObserver = new TabModelTabObserver(mTabModel);

        assertTrue(TabModelUtils.getCurrentTab(mTabModel).hasObserver(mTabModelTabObserver));
    }

    @Test
    @SmallTest
    public void testDestroyRemovesObservers() {
        mTabModel.addTab(1);
        mTabModelTabObserver.destroy();

        assertFalse(mTabModel.getTabAt(0).hasObserver(mTabModelTabObserver));
        assertFalse(mTabModel.getTabAt(1).hasObserver(mTabModelTabObserver));
    }
}
