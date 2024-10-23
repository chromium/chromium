// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs.features.branding;

import static org.junit.Assert.assertEquals;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import static org.chromium.chrome.browser.customtabs.features.branding.BrandingController.BRANDING_CADENCE_MS;
import static org.chromium.chrome.browser.customtabs.features.branding.BrandingController.MAX_BLANK_TOOLBAR_TIMEOUT_MS;

import android.content.Context;
import android.os.Handler;
import android.os.Looper;
import android.os.SystemClock;

import androidx.appcompat.view.ContextThemeWrapper;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Shadows;
import org.robolectric.annotation.Config;
import org.robolectric.annotation.LooperMode;
import org.robolectric.annotation.LooperMode.Mode;
import org.robolectric.shadows.ShadowLooper;
import org.robolectric.shadows.ShadowSystemClock;
import org.robolectric.shadows.ShadowToast;

import org.chromium.base.ContextUtils;
import org.chromium.base.FakeTimeTestRule;
import org.chromium.base.TimeUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.task.TaskTraits;
import org.chromium.base.task.test.ShadowPostTask;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.ui.widget.Toast;
import org.chromium.ui.widget.ToastManager;

import java.util.Locale;
import java.util.concurrent.TimeUnit;

/** Unit test for {@link BrandingController} and {@link SharedPreferencesBrandingTimeStorage}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(
        manifest = Config.NONE,
        shadows = {ShadowSystemClock.class, ShadowPostTask.class, ShadowToast.class})
@LooperMode(Mode.PAUSED)
public class BrandingControllerUnitTest {
    @Rule public MockitoRule mTestRule = MockitoJUnit.rule();
    @Rule public FakeTimeTestRule mFakeTimeTestRule = new FakeTimeTestRule();

    @Mock ToolbarBrandingDelegate mToolbarBrandingDelegate;

    private BrandingController mBrandingController;
    private ShadowPostTask.TestImpl mShadowPostTaskImpl;

    @Before
    public void setup() {
        mShadowPostTaskImpl =
                new ShadowPostTask.TestImpl() {
                    final Handler mHandler = new Handler(Looper.getMainLooper());

                    @Override
                    public void postDelayedTask(
                            @TaskTraits int taskTraits, Runnable task, long delay) {
                        mHandler.postDelayed(task, delay);
                    }
                };
        ShadowPostTask.setTestImpl(mShadowPostTaskImpl);

        SystemClock.setCurrentTimeMillis(TimeUtils.currentTimeMillis());
    }

    @After
    public void tearDown() {
        mFakeTimeTestRule.resetTimes();
        SharedPreferencesBrandingTimeStorage.getInstance().resetSharedPref();
        ShadowSystemClock.reset();
        ShadowToast.reset();
        ToastManager.resetForTesting();
    }

    @Test
    public void testBrandingWorkflow_FirstTime() {
        new BrandingCheckTester()
                .newBrandingController()
                .assertBrandingDecisionMade(null)
                .idleMainLooper() // Finish branding checker
                .assertBrandingDecisionMade(BrandingDecision.TOAST)
                .assertShownEmptyLocationBar(false)
                .onToolbarInitialized()
                .assertShownEmptyLocationBar(true)
                .assertShownBrandingLocationBar(false)
                .assertShownRegularLocationBar(true);

        ShadowLooper.idleMainLooper();
        assertTotalNumberOfPackageRecorded(1); // 1 new package
    }

    @Test
    public void testBrandingWorkflow_EmptyToolbarWithinCadence() {
        new BrandingCheckTester()
                .newBrandingController()
                .idleMainLooper() // Finish branding checker.
                .onToolbarInitialized()
                .assertShownToastBranding(true)
                .assertShownBrandingLocationBar(false)
                .assertShownRegularLocationBar(true)
                // Start 2nd branding immediately.
                .newBrandingController()
                .idleMainLooper()
                .assertBrandingDecisionMade(BrandingDecision.NONE)
                .onToolbarInitialized()
                .assertShownToastBranding(false)
                .assertShownEmptyLocationBar(true)
                .assertShownBrandingLocationBar(false)
                .assertShownRegularLocationBar(true);
    }

    @Test
    public void testBrandingWorkflow_ShowToolbarBranding() {
        new BrandingCheckTester()
                .newBrandingController()
                .idleMainLooper() // Finish branding checker.
                .assertBrandingDecisionMade(BrandingDecision.TOAST)
                .onToolbarInitialized()
                .assertShownToastBranding(true)
                .assertShownRegularLocationBar(true)
                // Start 2nd branding with delay.
                .advanceMills(BRANDING_CADENCE_MS + 1)
                .newBrandingController()
                .idleMainLooper() // Finish branding checker.
                .assertBrandingDecisionMade(BrandingDecision.TOOLBAR)
                .onToolbarInitialized()
                .assertShownBrandingLocationBar(true)
                .assertShownToastBranding(false)
                .advanceMills(BrandingController.TOTAL_BRANDING_DELAY_MS + 1)
                .idleMainLooper() // Finish toolbar branding
                .assertShownRegularLocationBar(true);
    }

    @Test
    public void testBrandingWorkflow_CheckDoneLaterThanToolbar() {
        new BrandingCheckTester()
                .newBrandingController()
                .onToolbarInitialized()
                .assertShownEmptyLocationBar(true)
                .assertBrandingDecisionMade(null)
                .assertShownBrandingLocationBar(false)
                .advanceMills(300)
                .idleMainLooper() // Finish branding checker.
                .assertBrandingDecisionMade(BrandingDecision.TOAST)
                .assertShownToastBranding(true)
                .assertShownRegularLocationBar(true);
    }

    @Test
    public void testBrandingWorkflow_CheckerTimeout() {
        new BrandingCheckTester()
                .newBrandingController()
                .onToolbarInitialized()
                .idleMainLooper()
                .assertBrandingDecisionMade(BrandingDecision.TOAST)
                .assertShownToastBranding(true)
                .newBrandingController()
                .onToolbarInitialized()
                .advanceMills(MAX_BLANK_TOOLBAR_TIMEOUT_MS)
                .idleMainLooper() // Branding checker is finished, but timed out comes first.
                .assertBrandingDecisionMade(BrandingDecision.TOAST)
                .assertShownRegularLocationBar(true);

        // BrandingController.TOTAL_BRANDING_DELAY_MS - MAX_BLANK_TOOLBAR_TIMEOUT_MS = 1300
        assertEquals(
                "Toast duration is different.",
                Toast.LENGTH_LONG,
                ShadowToast.getLatestToast().getDuration());
    }

    @Test
    public void testBrandingWorkflow_AuthTab() {
        new BrandingCheckTester()
                .newAuthTabBrandingController()
                .onToolbarInitialized()
                .idleMainLooper()
                .assertBrandingDecisionMade(BrandingDecision.TOAST)
                .assertShownToastBranding(true)
                .newAuthTabBrandingController()
                .idleMainLooper()
                .assertBrandingDecisionMade(BrandingDecision.TOAST)
                .onToolbarInitialized()
                .assertShownToastBranding(true);
    }

    @Test
    public void testDestroy() {
        // Inspired by https://crbug.com/1362437. Make sure callback are canceled once the branding
        // controller is destroyed.
        new BrandingCheckTester()
                .newBrandingController()
                .onToolbarInitialized()
                .destroyController()
                .idleMainLooper() // Finish Branding checker.
                // No toolbar update / branding toast should be shown.
                .assertShownBrandingLocationBar(false)
                .assertShownRegularLocationBar(false)
                .assertShownToastBranding(false);
    }

    @Test
    public void testDestroyed_ToolbarInitAfterDestroyed() {
        new BrandingCheckTester()
                .newBrandingController()
                .destroyController()
                .idleMainLooper() // Finish Branding checker
                .onToolbarInitialized()
                .assertShownBrandingLocationBar(false)
                .assertShownRegularLocationBar(false)
                .assertShownToastBranding(false);
    }

    @Test
    public void testDestroyed_ToastCanceled() {
        new BrandingCheckTester()
                .newBrandingController()
                .onToolbarInitialized()
                .idleMainLooper() // Finish Branding checker.
                .assertShownToastBranding(true)
                .destroyController();

        ShadowToast shadowToast = Shadows.shadowOf(ShadowToast.getLatestToast());
        Assert.assertTrue(
                "Toast should be canceled when controller destroyed.", shadowToast.isCancelled());
    }

    @Test
    public void testNumberOfPackageNames() {
        SharedPreferencesBrandingTimeStorage storage =
                SharedPreferencesBrandingTimeStorage.getInstance();
        storage.put("stubPackageA", 1L);
        storage.put("stubPackageB", 1L);
        storage.put("stubPackageC", 1L);
        ShadowLooper.idleMainLooper();
        assertEquals("3 Stub package name should be in the storage.", 3, storage.getSize());

        new BrandingCheckTester()
                .newBrandingController()
                .onToolbarInitialized()
                .idleMainLooper() // Finish Branding checker.
                .assertShownToastBranding(true);
        ShadowLooper.idleMainLooper();
        assertTotalNumberOfPackageRecorded(4); // 3 old package + 1 new package
    }

    class BrandingCheckTester {
        public BrandingCheckTester newBrandingController() {
            Context context = ContextUtils.getApplicationContext();
            mBrandingController =
                    new BrandingController(
                            new ContextThemeWrapper(context, R.style.Theme_Chromium_Activity),
                            "appName",
                            context.getPackageName(),
                            R.string.twa_running_in_chrome_template,
                            null);

            // Always initialize a new mock, as some tests were testing multiple branding runs.
            mToolbarBrandingDelegate = mock(ToolbarBrandingDelegate.class);
            ShadowToast.reset(); // Reset the shadow toast so the toast shown count resets.
            return this;
        }

        public BrandingCheckTester newAuthTabBrandingController() {
            Context context = ContextUtils.getApplicationContext();
            mBrandingController =
                    new BrandingController(
                            new ContextThemeWrapper(context, R.style.Theme_Chromium_Activity),
                            /* appId= */ null,
                            context.getPackageName(),
                            R.string.auth_tab_secured_by_chrome_template,
                            null);

            // Always initialize a new mock, as some tests were testing multiple branding runs.
            mToolbarBrandingDelegate = mock(ToolbarBrandingDelegate.class);
            ShadowToast.reset(); // Reset the shadow toast so the toast shown count resets.
            return this;
        }

        public BrandingCheckTester assertShownEmptyLocationBar(boolean shown) {
            verify(mToolbarBrandingDelegate, shown ? times(1) : never()).showEmptyLocationBar();
            return this;
        }

        public BrandingCheckTester assertShownBrandingLocationBar(boolean shown) {
            verify(mToolbarBrandingDelegate, shown ? times(1) : never()).showBrandingLocationBar();
            return this;
        }

        public BrandingCheckTester assertShownRegularLocationBar(boolean shown) {
            verify(mToolbarBrandingDelegate, shown ? times(1) : never()).showRegularToolbar();
            return this;
        }

        public BrandingCheckTester assertBrandingDecisionMade(@BrandingDecision Integer decision) {
            assertEquals(
                    "BrandingDecision is different.",
                    decision,
                    mBrandingController.getBrandingDecisionForTest());
            return this;
        }

        public BrandingCheckTester assertShownToastBranding(boolean shown) {
            assertEquals(
                    "Toast shown count does not match.",
                    shown ? 1 : 0,
                    ShadowToast.shownToastCount());
            ToastManager.resetForTesting();
            return this;
        }

        public BrandingCheckTester onToolbarInitialized() {
            mBrandingController.onToolbarInitialized(mToolbarBrandingDelegate);
            return this;
        }

        public BrandingCheckTester idleMainLooper() {
            Shadows.shadowOf(Looper.getMainLooper()).idle();
            return this;
        }

        public BrandingCheckTester advanceMills(long duration) {
            ShadowSystemClock.advanceBy(duration, TimeUnit.MILLISECONDS);
            mFakeTimeTestRule.advanceMillis(duration);
            return this;
        }

        public BrandingCheckTester destroyController() {
            mBrandingController.destroy();
            return this;
        }
    }

    private void assertTotalNumberOfPackageRecorded(int sample) {
        String histogram = "CustomTabs.Branding.NumberOfClients";
        assertEquals(
                String.format(Locale.US, "<%s> not recorded for count <%d>", histogram, sample),
                1,
                RecordHistogram.getHistogramValueCountForTesting(histogram, sample));
    }
}
