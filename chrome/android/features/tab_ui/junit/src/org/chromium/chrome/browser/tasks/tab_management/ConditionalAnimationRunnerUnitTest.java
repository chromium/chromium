// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.graphics.Bitmap;

import androidx.annotation.Nullable;
import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseRobolectricTestRunner;

/** Unit tests for {@link ConditionalAnimationRunner} */
@RunWith(BaseRobolectricTestRunner.class)
public class ConditionalAnimationRunnerUnitTest {
    private static class TestAnimationRunner implements ConditionalAnimationRunner.AnimationRunner {
        private @Nullable Bitmap mLastBitmap;
        private boolean mLastTabListCanShowQuickly;
        private int mTimesCalled;

        @Override
        public void run(@Nullable Bitmap bitmap, boolean tabListCanShowQuickly) {
            mTimesCalled++;
            mLastBitmap = bitmap;
            mLastTabListCanShowQuickly = tabListCanShowQuickly;
        }

        int getTimesCalled() {
            return mTimesCalled;
        }

        @Nullable
        Bitmap getBitmap() {
            return mLastBitmap;
        }

        boolean getTabListCanShowQuickly() {
            return mLastTabListCanShowQuickly;
        }
    }

    @Test
    @SmallTest
    public void testRunTabListCanShowQuicklyLayoutBitmap() {
        TestAnimationRunner runner = new TestAnimationRunner();
        ConditionalAnimationRunner conditionalRunner = new ConditionalAnimationRunner(runner);

        conditionalRunner.setTabListCanShowQuickly(true);
        Assert.assertEquals(runner.getTimesCalled(), 0);

        conditionalRunner.setLayoutCompleted();
        Assert.assertEquals(runner.getTimesCalled(), 0);

        conditionalRunner.setBitmap(null);
        Assert.assertEquals(runner.getTimesCalled(), 1);
        Assert.assertNull(runner.getBitmap());
        Assert.assertTrue(runner.getTabListCanShowQuickly());

        conditionalRunner.runAnimationDueToTimeout();
        Assert.assertEquals(runner.getTimesCalled(), 1);
    }

    @Test
    @SmallTest
    public void testRunTabListCanShowQuicklyBitmapLayout() {
        TestAnimationRunner runner = new TestAnimationRunner();
        ConditionalAnimationRunner conditionalRunner = new ConditionalAnimationRunner(runner);

        conditionalRunner.setTabListCanShowQuickly(false);
        Assert.assertEquals(runner.getTimesCalled(), 0);

        Bitmap bitmap = Bitmap.createBitmap(5, 10, Bitmap.Config.ALPHA_8);
        conditionalRunner.setBitmap(bitmap);
        Assert.assertEquals(runner.getTimesCalled(), 0);

        conditionalRunner.setLayoutCompleted();
        Assert.assertEquals(runner.getTimesCalled(), 1);
        Assert.assertEquals(runner.getBitmap(), bitmap);
        Assert.assertFalse(runner.getTabListCanShowQuickly());

        conditionalRunner.runAnimationDueToTimeout();
        Assert.assertEquals(runner.getTimesCalled(), 1);
    }

    @Test
    @SmallTest
    public void testRunBitmapTabListCanShowQuicklyLayout() {
        TestAnimationRunner runner = new TestAnimationRunner();
        ConditionalAnimationRunner conditionalRunner = new ConditionalAnimationRunner(runner);

        Bitmap bitmap = Bitmap.createBitmap(5, 10, Bitmap.Config.ALPHA_8);
        conditionalRunner.setBitmap(bitmap);
        Assert.assertEquals(runner.getTimesCalled(), 0);

        conditionalRunner.setTabListCanShowQuickly(true);
        Assert.assertEquals(runner.getTimesCalled(), 0);

        conditionalRunner.setLayoutCompleted();
        Assert.assertEquals(runner.getTimesCalled(), 1);
        Assert.assertEquals(runner.getBitmap(), bitmap);
        Assert.assertTrue(runner.getTabListCanShowQuickly());

        conditionalRunner.runAnimationDueToTimeout();
        Assert.assertEquals(runner.getTimesCalled(), 1);
    }

    @Test
    @SmallTest
    public void testRunBitmapLayoutTabListCanShowQuickly() {
        TestAnimationRunner runner = new TestAnimationRunner();
        ConditionalAnimationRunner conditionalRunner = new ConditionalAnimationRunner(runner);

        conditionalRunner.setBitmap(null);
        Assert.assertEquals(runner.getTimesCalled(), 0);

        conditionalRunner.setLayoutCompleted();
        Assert.assertEquals(runner.getTimesCalled(), 0);

        conditionalRunner.setTabListCanShowQuickly(false);
        Assert.assertEquals(runner.getTimesCalled(), 1);
        Assert.assertNull(runner.getBitmap());
        Assert.assertFalse(runner.getTabListCanShowQuickly());

        conditionalRunner.runAnimationDueToTimeout();
        Assert.assertEquals(runner.getTimesCalled(), 1);
    }

    @Test
    @SmallTest
    public void testForceRunOnTimeout() {
        TestAnimationRunner runner = new TestAnimationRunner();
        ConditionalAnimationRunner conditionalRunner = new ConditionalAnimationRunner(runner);

        conditionalRunner.runAnimationDueToTimeout();
        Assert.assertEquals(runner.getTimesCalled(), 1);
        Assert.assertNull(runner.getBitmap());
        Assert.assertFalse(runner.getTabListCanShowQuickly());
    }
}
