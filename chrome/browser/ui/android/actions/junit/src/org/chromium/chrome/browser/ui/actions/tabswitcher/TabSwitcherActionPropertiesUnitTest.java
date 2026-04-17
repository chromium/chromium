// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.actions.tabswitcher;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.ui.actions.ActionProperties;
import org.chromium.ui.modelutil.PropertyKey;

import java.util.Arrays;
import java.util.List;

/** Unit tests for {@link TabSwitcherActionProperties}. */
@RunWith(BaseRobolectricTestRunner.class)
public class TabSwitcherActionPropertiesUnitTest {

    @Test
    public void testValidKeys() {
        List<PropertyKey> keys = Arrays.asList(TabSwitcherActionProperties.ALL_KEYS);

        // Assert that ICON is not allowed to be modified directly.
        assertFalse("ICON should not be a valid key", keys.contains(ActionProperties.ICON));

        // Assert all valid keys are present.
        assertTrue(keys.contains(ActionProperties.CONTENT_DESCRIPTION_RESOLVER));
        assertTrue(keys.contains(ActionProperties.TOOLTIP_TEXT_RESOLVER));
        assertTrue(keys.contains(ActionProperties.ON_PRESS_CALLBACK));
        assertTrue(keys.contains(ActionProperties.ON_LONG_PRESS_CALLBACK));
        assertTrue(keys.contains(ActionProperties.IPH_INTENT));
        assertTrue(keys.contains(ActionProperties.USER_EDUCATION_HELPER));
        assertTrue(keys.contains(ActionProperties.BUTTON_STATE));
        assertTrue(keys.contains(TabSwitcherActionProperties.TAB_COUNT));
        assertTrue(keys.contains(TabSwitcherActionProperties.HAS_NOTIFICATION_DOT));
        assertTrue(keys.contains(TabSwitcherActionProperties.SHOW_TAB_SWITCHER_TRIGGER));
        assertTrue(keys.contains(TabSwitcherActionProperties.IS_INCOGNITO));

        assertEquals(11, keys.size());
    }
}
