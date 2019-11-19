// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.identity;

import android.support.test.InstrumentationRegistry;
import android.support.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.AdvancedMockContext;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;

@RunWith(ChromeJUnit4ClassRunner.class)
public class UuidBasedUniqueIdentificationGeneratorTest {
    private static final String FLAG_UUID = "uuid";

    private AdvancedMockContext mContext;

    @Before
    public void setUp() {
        mContext = new AdvancedMockContext(InstrumentationRegistry.getTargetContext());
    }

    @Test
    @SmallTest
    @Feature({"Sync"})
    public void testGenerationAndRestorationOfUuid() {
        String preferenceKey = "some_preference_key";
        String expectedUniqueId = "myUuid";
        TestGenerator generator = new TestGenerator(mContext, preferenceKey, expectedUniqueId);

        // Get a unique ID and ensure it is as expected.
        Assert.assertEquals(expectedUniqueId, generator.getUniqueId(null));

        // Asking for a unique ID again, should not try to regenerate it.
        mContext.clearFlag(FLAG_UUID);
        Assert.assertEquals(expectedUniqueId, generator.getUniqueId(null));
        Assert.assertFalse(mContext.isFlagSet(FLAG_UUID));

        // After a restart, the TestGenerator should read the UUID from a preference, instead of
        // asking for it.
        mContext.clearFlag(FLAG_UUID);
        generator = new TestGenerator(mContext, preferenceKey, null);
        Assert.assertEquals(expectedUniqueId, generator.getUniqueId(null));
        Assert.assertFalse(mContext.isFlagSet(FLAG_UUID));
    }

    @Test
    @SmallTest
    @Feature({"Sync"})
    public void testTwoDifferentGeneratorsShouldUseDifferentPreferences() {
        String preferenceKey1 = "some_preference_key";
        String preferenceKey2 = "some_other_preference_key";
        String expectedUniqueId1 = "myUuid";
        String expectedUniqueId2 = "myOtherUuid";
        TestGenerator generator1 = new TestGenerator(mContext, preferenceKey1, expectedUniqueId1);
        TestGenerator generator2 = new TestGenerator(mContext, preferenceKey2, expectedUniqueId2);

        // Get a unique ID and ensure it is as expected.
        Assert.assertEquals(expectedUniqueId1, generator1.getUniqueId(null));
        Assert.assertEquals(expectedUniqueId2, generator2.getUniqueId(null));

        // Asking for a unique ID again, should not try to regenerate it.
        mContext.clearFlag(FLAG_UUID);
        Assert.assertEquals(expectedUniqueId1, generator1.getUniqueId(null));
        Assert.assertFalse(mContext.isFlagSet(FLAG_UUID));
        mContext.clearFlag(FLAG_UUID);
        Assert.assertEquals(expectedUniqueId2, generator2.getUniqueId(null));
        Assert.assertFalse(mContext.isFlagSet(FLAG_UUID));
    }

    private static class TestGenerator extends UuidBasedUniqueIdentificationGenerator {
        private final AdvancedMockContext mContext;
        private final String mUuid;

        TestGenerator(AdvancedMockContext context, String preferenceKey, String uuid) {
            super(context, preferenceKey);
            mContext = context;
            mUuid = uuid;
        }

        @Override
        String getUUID() {
            mContext.setFlag(FLAG_UUID);
            return mUuid;
        }
    }
}
