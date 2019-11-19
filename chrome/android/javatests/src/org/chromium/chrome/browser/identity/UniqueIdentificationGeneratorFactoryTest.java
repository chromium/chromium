// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.identity;

import android.support.test.filters.SmallTest;

import androidx.annotation.Nullable;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.Feature;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;

@RunWith(ChromeJUnit4ClassRunner.class)
public class UniqueIdentificationGeneratorFactoryTest {
    @Test
    @SmallTest
    @Feature({"ChromeToMobile", "Omaha", "Sync"})
    public void testSetAndGetGenerator() {
        UniqueIdentificationGeneratorFactory.clearGeneratorMapForTest();
        UniqueIdentificationGenerator gen = new TestGenerator();
        UniqueIdentificationGeneratorFactory.registerGenerator("generator", gen, false);
        Assert.assertEquals(gen, UniqueIdentificationGeneratorFactory.getInstance("generator"));
    }

    @Test
    @SmallTest
    @Feature({"ChromeToMobile", "Omaha", "Sync"})
    public void testForceCanOverrideGenerator() {
        UniqueIdentificationGeneratorFactory.clearGeneratorMapForTest();
        UniqueIdentificationGenerator gen1 = new TestGenerator();
        UniqueIdentificationGenerator gen2 = new TestGenerator();
        UniqueIdentificationGenerator gen3 = new TestGenerator();
        UniqueIdentificationGeneratorFactory.registerGenerator("generator", gen1, false);
        Assert.assertEquals(gen1, UniqueIdentificationGeneratorFactory.getInstance("generator"));
        UniqueIdentificationGeneratorFactory.registerGenerator("generator", gen2, false);
        Assert.assertEquals(gen1, UniqueIdentificationGeneratorFactory.getInstance("generator"));
        UniqueIdentificationGeneratorFactory.registerGenerator("generator", gen3, true);
        Assert.assertEquals(gen3, UniqueIdentificationGeneratorFactory.getInstance("generator"));
    }

    @Test
    @SmallTest
    @Feature({"ChromeToMobile", "Omaha", "Sync"})
    public void testGeneratorNotFoundThrows() {
        UniqueIdentificationGeneratorFactory.clearGeneratorMapForTest();
        UniqueIdentificationGenerator generator = null;
        try {
            generator = UniqueIdentificationGeneratorFactory.getInstance("generator");
            Assert.fail("The generator does not exist, so factory should throw an error.");
        } catch (RuntimeException e) {
            Assert.assertEquals(null, generator);
        }
    }

    private static class TestGenerator implements UniqueIdentificationGenerator {
        @Override
        public String getUniqueId(@Nullable String salt) {
            return null;
        }
    }
}
