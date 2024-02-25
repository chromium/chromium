// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base;

import android.app.Application;
import android.content.ComponentCallbacks;
import android.content.ComponentCallbacks2;
import android.content.Context;
import android.content.res.Configuration;

import androidx.test.InstrumentationRegistry;
import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.AdvancedMockContext;

/** Tests for {@link org.chromium.base.test.util.AdvancedMockContext}. */
@RunWith(BaseJUnit4ClassRunner.class)
public class AdvancedMockContextTest {
    private static class Callback1 implements ComponentCallbacks {
        protected Configuration mConfiguration;
        protected boolean mOnLowMemoryCalled;

        @Override
        public void onConfigurationChanged(Configuration configuration) {
            mConfiguration = configuration;
        }

        @Override
        public void onLowMemory() {
            mOnLowMemoryCalled = true;
        }
    }

    private static class Callback2 extends Callback1 implements ComponentCallbacks2 {
        private int mLevel;

        @Override
        public void onTrimMemory(int level) {
            mLevel = level;
        }
    }

    @Test
    @SmallTest
    public void testComponentCallbacksForTargetContext() {
        Context targetContext = InstrumentationRegistry.getInstrumentation().getTargetContext();
        Application targetApplication = BaseJUnit4ClassRunner.getApplication();
        AdvancedMockContext context = new AdvancedMockContext(targetContext);
        Callback1 callback1 = new Callback1();
        Callback2 callback2 = new Callback2();
        context.registerComponentCallbacks(callback1);
        context.registerComponentCallbacks(callback2);

        targetApplication.onLowMemory();
        Assert.assertTrue("onLowMemory should have been called.", callback1.mOnLowMemoryCalled);
        Assert.assertTrue("onLowMemory should have been called.", callback2.mOnLowMemoryCalled);

        Configuration configuration = new Configuration();
        targetApplication.onConfigurationChanged(configuration);
        Assert.assertEquals(
                "onConfigurationChanged should have been called.",
                configuration,
                callback1.mConfiguration);
        Assert.assertEquals(
                "onConfigurationChanged should have been called.",
                configuration,
                callback2.mConfiguration);

        targetApplication.onTrimMemory(ComponentCallbacks2.TRIM_MEMORY_MODERATE);
        Assert.assertEquals(
                "onTrimMemory should have been called.",
                ComponentCallbacks2.TRIM_MEMORY_MODERATE,
                callback2.mLevel);
    }
}
