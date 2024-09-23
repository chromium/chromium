// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.hub;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNull;

import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseRobolectricTestRunner;

/** Unit tests for {@link HubShowPaneHelper}. */
@RunWith(BaseRobolectricTestRunner.class)
public class HubShowPaneHelperUnitTest {
    private HubShowPaneHelper mHubShowPaneHelper;

    @Before
    public void setUp() {
        mHubShowPaneHelper = new HubShowPaneHelper();
    }

    @Test
    @SmallTest
    public void testGetNextPaneIdWithDefaultValue() {
        assertNull(mHubShowPaneHelper.getNextPaneIdForTesting());

        boolean isIncognito = true;
        assertEquals(PaneId.INCOGNITO_TAB_SWITCHER, mHubShowPaneHelper.getNextPaneId(isIncognito));
        isIncognito = false;
        assertEquals(PaneId.TAB_SWITCHER, mHubShowPaneHelper.getNextPaneId(isIncognito));
    }

    @Test
    @SmallTest
    public void testSetNextPaneId() {
        assertNull(mHubShowPaneHelper.getNextPaneIdForTesting());
        mHubShowPaneHelper.setPaneToShow(PaneId.TAB_GROUPS);

        boolean isIncognito = true;
        assertEquals(PaneId.INCOGNITO_TAB_SWITCHER, mHubShowPaneHelper.getNextPaneId(isIncognito));
        isIncognito = false;
        assertEquals(PaneId.TAB_GROUPS, mHubShowPaneHelper.getNextPaneId(isIncognito));
    }

    @Test
    @SmallTest
    public void testConsumeNextPaneId() {
        mHubShowPaneHelper.setPaneToShow(PaneId.TAB_GROUPS);

        boolean isIncognito = true;
        // Verifies that consumeNextPaneId always return PaneId.INCOGNITO_TAB_SWITCHER in incognito
        // mode.
        assertEquals(
                PaneId.INCOGNITO_TAB_SWITCHER, mHubShowPaneHelper.consumeNextPaneId(isIncognito));

        isIncognito = false;
        mHubShowPaneHelper.setPaneToShow(PaneId.TAB_GROUPS);
        assertEquals(PaneId.TAB_GROUPS, mHubShowPaneHelper.consumeNextPaneId(isIncognito));
        // Verifies that the next pane id is reset to null.
        assertNull(mHubShowPaneHelper.getNextPaneIdForTesting());
        assertEquals(PaneId.TAB_SWITCHER, mHubShowPaneHelper.getNextPaneId(isIncognito));
    }
}
