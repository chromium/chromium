// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.dom_distiller;

import androidx.test.annotation.UiThreadTest;
import androidx.test.filters.SmallTest;

import com.google.common.util.concurrent.AtomicDouble;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.ClassRule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.test.ChromeBrowserTestRule;
import org.chromium.components.dom_distiller.core.DistilledPagePrefs;
import org.chromium.components.dom_distiller.core.DomDistillerService;
import org.chromium.dom_distiller.mojom.FontFamily;
import org.chromium.dom_distiller.mojom.Theme;

import java.util.concurrent.Semaphore;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.atomic.AtomicBoolean;
import java.util.concurrent.atomic.AtomicInteger;

/** Test class for {@link DistilledPagePrefs}. */
@RunWith(BaseJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
public class DistilledPagePrefsTest {
    @ClassRule
    public static final ChromeBrowserTestRule sChromeBrowserTestRule = new ChromeBrowserTestRule();

    private DistilledPagePrefs mDistilledPagePrefs;

    private static final double EPSILON = 1e-5;

    private static final TimeUnit SEMAPHORE_TIMEOUT_UNIT = TimeUnit.SECONDS;
    private static final long SEMAPHORE_TIMEOUT_VALUE = 5;

    @Before
    public void setUp() {
        getDistilledPagePrefs();
    }

    @After
    public void tearDown() {
        // Set back to default theme
        setTheme(Theme.LIGHT);
    }

    private void getDistilledPagePrefs() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    // TODO (https://crbug.com/1063807):  Add incognito mode tests.
                    DomDistillerService domDistillerService =
                            DomDistillerServiceFactory.getForProfile(
                                    ProfileManager.getLastUsedRegularProfile());
                    mDistilledPagePrefs = domDistillerService.getDistilledPagePrefs();
                });
    }

    @Test
    @SmallTest
    @UiThreadTest
    @Feature({"DomDistiller"})
    public void testGetAndSetTheme() {
        // Check the default theme.
        Assert.assertEquals(Theme.LIGHT, mDistilledPagePrefs.getTheme());
        // Check that theme can be correctly set.
        setTheme(Theme.DARK);
        Assert.assertEquals(Theme.DARK, mDistilledPagePrefs.getTheme());
        setTheme(Theme.LIGHT);
        Assert.assertEquals(Theme.LIGHT, mDistilledPagePrefs.getTheme());
        setTheme(Theme.SEPIA);
        Assert.assertEquals(Theme.SEPIA, mDistilledPagePrefs.getTheme());
    }

    @Test
    @SmallTest
    @Feature({"DomDistiller"})
    public void testSingleObserverTheme() throws InterruptedException {
        TestingObserver testObserver = new TestingObserver();
        addObserver(testObserver);

        Assert.assertEquals(Theme.LIGHT, testObserver.getTheme());
        setTheme(Theme.DARK);
        // Check that testObserver's theme has been updated,
        Assert.assertEquals(Theme.DARK, testObserver.getThemeAfterWaiting());
        removeObserver(testObserver);
    }

    @Test
    @SmallTest
    @Feature({"DomDistiller"})
    public void testMultipleObserversTheme() throws InterruptedException {
        TestingObserver testObserverOne = new TestingObserver();
        addObserver(testObserverOne);
        TestingObserver testObserverTwo = new TestingObserver();
        addObserver(testObserverTwo);

        setTheme(Theme.SEPIA);
        Assert.assertEquals(Theme.SEPIA, testObserverOne.getThemeAfterWaiting());
        Assert.assertEquals(Theme.SEPIA, testObserverTwo.getThemeAfterWaiting());
        removeObserver(testObserverOne);

        setTheme(Theme.DARK);
        // Check that testObserverOne's theme is not changed but testObserverTwo's is.
        Assert.assertEquals(Theme.DARK, testObserverTwo.getThemeAfterWaiting());
        // There is no simple way to safely wait for something not to happen unless we force a timed
        // wait, which would slow down test runs. Since testObserverTwo has been invoked, we
        // incorrectly assume that testObserverOne would have been as well to keep the test runtime
        // short.
        Assert.assertEquals(Theme.SEPIA, testObserverOne.getTheme());
        removeObserver(testObserverTwo);
    }

    @Test
    @SmallTest
    @UiThreadTest
    @Feature({"DomDistiller"})
    public void testGetAndSetFontFamily() {
        // Check the default font family.
        Assert.assertEquals(FontFamily.SANS_SERIF, mDistilledPagePrefs.getFontFamily());
        // Check that font family can be correctly set.
        setFontFamily(FontFamily.SERIF);
        Assert.assertEquals(FontFamily.SERIF, mDistilledPagePrefs.getFontFamily());
    }

    @Test
    @SmallTest
    @Feature({"DomDistiller"})
    public void testSingleObserverFontFamily() throws InterruptedException {
        TestingObserver testObserver = new TestingObserver();
        addObserver(testObserver);

        Assert.assertEquals(FontFamily.SANS_SERIF, testObserver.getFontFamily());
        setFontFamily(FontFamily.SERIF);
        // Check that testObserver's font family has been updated.
        Assert.assertEquals(FontFamily.SERIF, testObserver.getFontFamilyAfterWaiting());
        removeObserver(testObserver);
    }

    @Test
    @SmallTest
    @Feature({"DomDistiller"})
    public void testMultipleObserversFontFamily() throws InterruptedException {
        TestingObserver testObserverOne = new TestingObserver();
        addObserver(testObserverOne);
        TestingObserver testObserverTwo = new TestingObserver();
        addObserver(testObserverTwo);

        setFontFamily(FontFamily.MONOSPACE);
        Assert.assertEquals(FontFamily.MONOSPACE, testObserverOne.getFontFamilyAfterWaiting());
        Assert.assertEquals(FontFamily.MONOSPACE, testObserverTwo.getFontFamilyAfterWaiting());
        removeObserver(testObserverOne);

        setFontFamily(FontFamily.SERIF);
        // Check that testObserverOne's font family is not changed but testObserverTwo's is.
        Assert.assertEquals(FontFamily.SERIF, testObserverTwo.getFontFamilyAfterWaiting());
        // There is no simple way to safely wait for something not to happen unless we force a timed
        // wait, which would slow down test runs. Since testObserverTwo has been invoked, we
        // incorrectly assume that testObserverOne would have been as well to keep the test runtime
        // short.
        Assert.assertEquals(FontFamily.MONOSPACE, testObserverOne.getFontFamily());
        removeObserver(testObserverTwo);
    }

    @Test
    @SmallTest
    @UiThreadTest
    @Feature({"DomDistiller"})
    public void testGetAndSetFontScaling() {
        // Check the default font scaling.
        Assert.assertEquals(1.0, mDistilledPagePrefs.getFontScaling(), EPSILON);
        // Check that font scaling can be correctly set.
        setFontScaling(1.2f);
        Assert.assertEquals(1.2, mDistilledPagePrefs.getFontScaling(), EPSILON);
    }

    @Test
    @SmallTest
    @Feature({"DomDistiller"})
    public void testSingleObserverFontScaling() throws InterruptedException {
        TestingObserver testObserver = new TestingObserver();
        addObserver(testObserver);

        Assert.assertNotEquals(1.1, testObserver.getFontScaling(), EPSILON);
        setFontScaling(1.1f);
        // Check that testObserver's font scaling has been updated.
        Assert.assertEquals(1.1, testObserver.getFontScalingAfterWaiting(), EPSILON);
        removeObserver(testObserver);
    }

    @Test
    @SmallTest
    @Feature({"DomDistiller"})
    public void testMultipleObserversFontScaling() throws InterruptedException {
        TestingObserver testObserverOne = new TestingObserver();
        addObserver(testObserverOne);
        TestingObserver testObserverTwo = new TestingObserver();
        addObserver(testObserverTwo);

        setFontScaling(1.3f);
        Assert.assertEquals(1.3, testObserverOne.getFontScalingAfterWaiting(), EPSILON);
        Assert.assertEquals(1.3, testObserverTwo.getFontScalingAfterWaiting(), EPSILON);
        removeObserver(testObserverOne);

        setFontScaling(0.9f);
        // Check that testObserverOne's font scaling is not changed but testObserverTwo's is.
        Assert.assertEquals(0.9, testObserverTwo.getFontScalingAfterWaiting(), EPSILON);
        // There is no simple way to safely wait for something not to happen unless we force a timed
        // wait, which would slow down test runs. Since testObserverTwo has been invoked, we
        // incorrectly assume that testObserverOne would have been as well to keep the test runtime
        // short.
        Assert.assertEquals(1.3, testObserverOne.getFontScaling(), EPSILON);
        removeObserver(testObserverTwo);
    }

    @Test
    @SmallTest
    @Feature({"DomDistiller"})
    public void testRepeatedAddAndDeleteObserver() {
        TestingObserver test = new TestingObserver();

        // Should successfully add the observer the first time.
        Assert.assertTrue(addObserver(test));
        // Observer cannot be added again, should return false.
        Assert.assertFalse(addObserver(test));

        // Delete the observer the first time.
        Assert.assertTrue(removeObserver(test));
        // Observer cannot be deleted again, should return false.
        Assert.assertFalse(removeObserver(test));
    }

    private static class TestingObserver implements DistilledPagePrefs.Observer {
        private final AtomicInteger mFontFamily = new AtomicInteger();
        private Semaphore mFontFamilySemaphore = new Semaphore(0);
        private final AtomicInteger mTheme = new AtomicInteger();
        private Semaphore mThemeSemaphore = new Semaphore(0);
        private final AtomicDouble mFontScaling = new AtomicDouble();
        private Semaphore mFontScalingSemaphore = new Semaphore(0);

        public TestingObserver() {}

        public int getFontFamily() {
            return mFontFamily.get();
        }

        public int getFontFamilyAfterWaiting() throws InterruptedException {
            Assert.assertTrue(
                    "Did not receive an update for font family",
                    mFontFamilySemaphore.tryAcquire(
                            SEMAPHORE_TIMEOUT_VALUE, SEMAPHORE_TIMEOUT_UNIT));
            return getFontFamily();
        }

        @Override
        public void onChangeFontFamily(int font) {
            mFontFamily.set(font);
            mFontFamilySemaphore.release();
        }

        public int getTheme() {
            return mTheme.get();
        }

        public int getThemeAfterWaiting() throws InterruptedException {
            Assert.assertTrue(
                    "Did not receive an update for theme",
                    mThemeSemaphore.tryAcquire(SEMAPHORE_TIMEOUT_VALUE, SEMAPHORE_TIMEOUT_UNIT));
            return getTheme();
        }

        @Override
        public void onChangeTheme(int theme) {
            mTheme.set(theme);
            mThemeSemaphore.release();
        }

        public float getFontScaling() {
            return (float) mFontScaling.get();
        }

        public float getFontScalingAfterWaiting() throws InterruptedException {
            Assert.assertTrue(
                    "Did not receive an update for font scaling",
                    mFontScalingSemaphore.tryAcquire(
                            SEMAPHORE_TIMEOUT_VALUE, SEMAPHORE_TIMEOUT_UNIT));
            return getFontScaling();
        }

        @Override
        public void onChangeFontScaling(float scaling) {
            mFontScaling.set(scaling);
            mFontScalingSemaphore.release();
        }
    }

    private void setFontFamily(final int font) {
        ThreadUtils.runOnUiThreadBlocking(() -> mDistilledPagePrefs.setFontFamily(font));
    }

    private void setTheme(final int theme) {
        ThreadUtils.runOnUiThreadBlocking(() -> mDistilledPagePrefs.setTheme(theme));
    }

    private void setFontScaling(final float scaling) {
        ThreadUtils.runOnUiThreadBlocking(() -> mDistilledPagePrefs.setFontScaling(scaling));
    }

    private boolean removeObserver(TestingObserver testObserver) {
        AtomicBoolean wasRemoved = new AtomicBoolean();
        ThreadUtils.runOnUiThreadBlocking(
                () -> wasRemoved.set(mDistilledPagePrefs.removeObserver(testObserver)));
        return wasRemoved.get();
    }

    private boolean addObserver(TestingObserver testObserver) {
        AtomicBoolean wasAdded = new AtomicBoolean();
        ThreadUtils.runOnUiThreadBlocking(
                () -> wasAdded.set(mDistilledPagePrefs.addObserver(testObserver)));
        return wasAdded.get();
    }
}
