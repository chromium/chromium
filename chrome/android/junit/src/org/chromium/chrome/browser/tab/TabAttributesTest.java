// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.when;

import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.UserDataHost;
import org.chromium.base.test.BaseRobolectricTestRunner;

import java.lang.reflect.Field;
import java.lang.reflect.Modifier;

/** Tests for {@link TabAttributes}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class TabAttributesTest {
    private static final String ATTR1 = "attr1";

    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Mock private Tab mTab;

    private final UserDataHost mUserDataHost = new UserDataHost();

    // User-defined class used for attribute type.
    private static class TestObject {}

    @Before
    public void setUp() {
        when(mTab.getUserDataHost()).thenReturn(mUserDataHost);
    }

    @Test
    @SmallTest
    public void testBasicGetAndSetOperation() {
        // |get| for an uninitialized attribute returns null.
        assertNull(TabAttributes.from(mTab).get(ATTR1));

        // |get| with a default value returns the given default.
        assertFalse(TabAttributes.from(mTab).get(ATTR1, false));

        // |get| returns the stored attribute.
        TabAttributes.from(mTab).set(ATTR1, true);
        assertTrue(TabAttributes.from(mTab).get(ATTR1));

        // |get| returns null after cleared.
        TabAttributes.from(mTab).clear(ATTR1);
        assertNull(TabAttributes.from(mTab).get(ATTR1));
    }

    @Test
    @SmallTest
    public void testGetWithDefaultReturnsNullForAttributeExplicitlySetToNull() {
        TestObject defaultValue = new TestObject();

        // The attribute is not set by default, therefore default value is returned.
        assertEquals(defaultValue, TabAttributes.from(mTab).get(ATTR1, defaultValue));

        // Explicitly set the attribute to null. Now |get| should return null,
        // disregarding the default value.
        TabAttributes.from(mTab).set(ATTR1, null);
        assertNull(TabAttributes.from(mTab).get(ATTR1, defaultValue));
    }

    @Test
    @SmallTest
    public void testNumEntriesMatchesKeys() {
        int keyCount = 0;
        for (Field field : TabAttributeKeys.class.getDeclaredFields()) {
            if (field.getType().equals(String.class)
                    && Modifier.isStatic(field.getModifiers())
                    && Modifier.isFinal(field.getModifiers())) {
                keyCount++;
            }
        }
        assertEquals(
                "TabAttributeKeys.NUM_ENTRIES does not match the number of String keys",
                TabAttributeKeys.NUM_ENTRIES,
                keyCount);
    }
}
