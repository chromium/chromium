// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab;

import static org.mockito.Mockito.when;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.UserDataHost;
import org.chromium.base.test.BaseRobolectricTestRunner;

/** Tests for {@link TabAttributes}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class TabAttributesTest {
    private static final String ATTR1 = "attr1";

    @Mock private Tab mTab;

    private final UserDataHost mUserDataHost = new UserDataHost();

    // User-defined class used for attribute type.
    private static class TestObject {}

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        when(mTab.getUserDataHost()).thenReturn(mUserDataHost);
    }

    @Test
    @SmallTest
    public void testBasicGetAndSetOperation() {
        // |get| for an uninitialized attribute returns null.
        Assert.assertNull(TabAttributes.from(mTab).get(ATTR1));

        // |get| with a default value returns the given default.
        Assert.assertFalse(TabAttributes.from(mTab).get(ATTR1, Boolean.FALSE));

        // |get| returns the stored attribute.
        TabAttributes.from(mTab).set(ATTR1, true);
        Assert.assertTrue(TabAttributes.from(mTab).get(ATTR1));

        // |get| returns null after cleared.
        TabAttributes.from(mTab).clear(ATTR1);
        Assert.assertNull(TabAttributes.from(mTab).get(ATTR1));
    }

    @Test
    @SmallTest
    public void testGetWithDefaultReturnsNullForAttributeExplicitlySetToNull() {
        TestObject defaultValue = new TestObject();

        // The attribute is not set by default, therefore default value is returned.
        Assert.assertEquals(defaultValue, TabAttributes.from(mTab).get(ATTR1, defaultValue));

        // Explicitly set the attribute to null. Now |get| should return null,
        // disregarding the default value.
        TabAttributes.from(mTab).set(ATTR1, null);
        Assert.assertNull(TabAttributes.from(mTab).get(ATTR1, defaultValue));
    }
}
