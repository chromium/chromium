// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.hub;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertNotNull;

import android.content.Context;

import androidx.test.core.app.ApplicationProvider;
import androidx.test.filters.SmallTest;

import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseRobolectricTestRunner;

/** Unit tests for {@link ResourceButtonData}. */
@RunWith(BaseRobolectricTestRunner.class)
public class ResourceButtonDataUnitTest {
    @Test
    @SmallTest
    public void testResolveTextAndIconAndContentDescription() {
        Context context = ApplicationProvider.getApplicationContext();
        DisplayButtonData buttonData =
                new ResourceButtonData(
                        R.string.button_new_tab,
                        R.string.button_new_incognito_tab,
                        R.drawable.ic_add);
        assertNotEquals(0, buttonData.resolveText(context).length());
        assertNotNull(buttonData.resolveIcon(context));
    }

    @Test
    @SmallTest
    public void testHashCode() {
        DisplayButtonData buttonData1 =
                new ResourceButtonData(
                        R.string.button_new_tab,
                        R.string.button_new_incognito_tab,
                        R.drawable.ic_add);
        DisplayButtonData buttonData2 =
                new ResourceButtonData(
                        R.string.button_new_tab,
                        R.string.button_new_incognito_tab,
                        R.drawable.ic_add);
        // Only test positive case, since we're not guaranteed to get different hash codes for
        // different values.
        assertEquals(buttonData1.hashCode(), buttonData2.hashCode());
    }

    @Test
    @SmallTest
    public void testEquals() {
        DisplayButtonData buttonData =
                new ResourceButtonData(
                        R.string.button_new_tab,
                        R.string.button_new_incognito_tab,
                        R.drawable.ic_add);
        assertEquals(
                buttonData,
                new ResourceButtonData(
                        R.string.button_new_tab,
                        R.string.button_new_incognito_tab,
                        R.drawable.ic_add));
        assertNotEquals(
                buttonData,
                new ResourceButtonData(
                        R.string.button_new_incognito_tab,
                        R.string.button_new_incognito_tab,
                        R.drawable.ic_add));
        assertNotEquals(
                buttonData,
                new ResourceButtonData(
                        R.string.button_new_tab,
                        R.string.button_new_incognito_tab,
                        R.drawable.ic_history_24dp));
        assertNotEquals(
                buttonData,
                new ResourceButtonData(
                        R.string.button_new_tab, R.string.button_new_tab, R.drawable.ic_add));

        // assert*Equals will not invoke #equals on a null object, manually call it instead.
        assertFalse(buttonData.equals(null));

        assertNotEquals(buttonData, new Object());
    }
}
