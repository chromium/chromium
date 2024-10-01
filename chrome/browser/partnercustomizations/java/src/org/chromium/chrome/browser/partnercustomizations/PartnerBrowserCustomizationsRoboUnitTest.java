// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.partnercustomizations;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import android.os.Handler;
import android.os.Looper;

import androidx.annotation.Nullable;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;
import org.robolectric.annotation.Implementation;
import org.robolectric.annotation.Implements;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.ContextUtils;
import org.chromium.base.task.TaskTraits;
import org.chromium.base.task.test.ShadowPostTask;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.partnercustomizations.PartnerBrowserCustomizationsRoboUnitTest.ShadowCustomizationProviderDelegate;
import org.chromium.chrome.browser.partnercustomizations.PartnerCustomizationsTestUtils.HomepageCharacterizationHelperStub;
import org.chromium.chrome.browser.partnercustomizations.PartnerCustomizationsUma.PartnerCustomizationsHomepageEnum;
import org.chromium.url.JUnitTestGURLs;

/** Unit tests for {@link PartnerBrowserCustomizations}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(shadows = {ShadowPostTask.class, ShadowCustomizationProviderDelegate.class})
@EnableFeatures(ChromeFeatureList.PARTNER_CUSTOMIZATIONS_UMA)
public class PartnerBrowserCustomizationsRoboUnitTest {
    @Mock private ActivityLifecycleDispatcher mActivityLifecycleDispatcherMock;

    @Before
    public void setup() {
        ShadowCustomizationProviderDelegate.sHomepage = JUnitTestGURLs.EXAMPLE_URL.getSpec();
        ShadowPostTask.setTestImpl(
                new ShadowPostTask.TestImpl() {
                    final Handler mHandler = new Handler(Looper.getMainLooper());

                    @Override
                    public void postDelayedTask(
                            @TaskTraits int taskTraits, Runnable task, long delay) {
                        mHandler.postDelayed(task, delay);
                    }
                });

        MockitoAnnotations.openMocks(this);
    }

    @After
    public void tearDown() {
        PartnerBrowserCustomizations.destroy();
        PartnerCustomizationsUma.resetStaticsForTesting();
    }

    @Test
    public void initializeAsyncOneAtATime() {
        PartnerBrowserCustomizations.getInstance()
                .initializeAsync(ContextUtils.getApplicationContext());
        // Run one task, so AsyncTask#doInBackground is run, but not #onFinalized.
        ShadowLooper.runMainLooperOneTask();
        assertFalse(
                "The homepage refreshed, but result is not yet posted on UI thread.",
                PartnerBrowserCustomizations.getInstance().isInitialized());

        // Assuming homepage is changed during 1st #initializeAsync, and another #initializeAsync is
        // triggered. The 2nd #initializeAsync is ignored since there's one already in the process.
        ShadowCustomizationProviderDelegate.sHomepage = JUnitTestGURLs.GOOGLE_URL.getSpec();
        PartnerBrowserCustomizations.getInstance()
                .initializeAsync(ContextUtils.getApplicationContext());
        assertFalse(
                "#initializeAsync should be in progress.",
                PartnerBrowserCustomizations.getInstance().isInitialized());

        ShadowLooper.idleMainLooper();
        assertTrue(
                "#initializeAsync should be done.",
                PartnerBrowserCustomizations.getInstance().isInitialized());
        assertEquals(
                "Homepage should be set via 1st initializeAsync.",
                JUnitTestGURLs.EXAMPLE_URL.getSpec(),
                PartnerBrowserCustomizations.getInstance().getHomePageUrl().getSpec());

        PartnerBrowserCustomizations.getInstance()
                .initializeAsync(ContextUtils.getApplicationContext());
        assertFalse(
                "3rd #initializeAsync should be in progress.",
                PartnerBrowserCustomizations.getInstance().isInitialized());

        ShadowLooper.idleMainLooper();
        assertTrue(
                "3rd #initializeAsync should be done.",
                PartnerBrowserCustomizations.getInstance().isInitialized());
        assertEquals(
                "Homepage should refreshed by 3rd #initializeAsync.",
                JUnitTestGURLs.GOOGLE_URL.getSpec(),
                PartnerBrowserCustomizations.getInstance().getHomePageUrl().getSpec());
    }

    /** Convenient shadow class to set homepage provided by partner during robo tests. */
    @Implements(CustomizationProviderDelegateUpstreamImpl.class)
    public static class ShadowCustomizationProviderDelegate {
        static String sHomepage;

        public ShadowCustomizationProviderDelegate() {}

        /** Returns the homepage string or null if none is available. */
        @Implementation
        @Nullable
        protected String getHomepage() {
            return sHomepage;
        }

        /** Returns whether incognito mode is disabled. */
        @Implementation
        protected boolean isIncognitoModeDisabled() {
            return false;
        }

        /** Returns whether bookmark editing is disabled. */
        @Implementation
        protected boolean isBookmarksEditingDisabled() {
            return false;
        }
    }

    /**
     * Tests the case where we think we're "certain" whether the NTP was created correctly or not.
     * We already created the Tab so when we complete customization we're ready to log to UMA, but
     * the ordering is critical. We need to call {@link
     * PartnerCustomizationsUma#logAsyncInitCompleted} before we finalize. Ordering: Create NTP,
     * customization says NTP, so it's either NTP_CORRECTLY (2) or NTP_UNKNOWN (0). This test will
     * fail if the ordering is changed in the {@code onPostExecute} handler inside {@link
     * PartnerBrowserCustomizations}.
     */
    @Test
    public void initializeAsyncWithPartnerCustomizationsUma() {
        ShadowCustomizationProviderDelegate.sHomepage = null;
        HistogramWatcher histograms =
                HistogramWatcher.newSingleRecordWatcher(
                        "Android.PartnerCustomization.HomepageCustomizationOutcome",
                        PartnerCustomizationsHomepageEnum.NTP_CORRECTLY);

        // Setup the Async task and get it running.
        PartnerBrowserCustomizations.getInstance()
                .initializeAsync(ContextUtils.getApplicationContext());
        // Run one task, so AsyncTask#doInBackground is run, but not #onFinalized.
        ShadowLooper.runMainLooperOneTask();

        // Simulate CTA#createInitialTab: Create an NTP when the delegate says Partner Homepage.
        // TODO(donnd): call this as an async callback through setOnInitializeAsyncFinished.
        PartnerBrowserCustomizations.getInstance()
                .onCreateInitialTab(
                        JUnitTestGURLs.NTP_NATIVE_URL.getSpec(),
                        mActivityLifecycleDispatcherMock,
                        HomepageCharacterizationHelperStub::ntpHelper);

        // Trigger Async completion.
        ShadowLooper.idleMainLooper();

        // Make sure the Outcome logged to UMA is correct.
        histograms.assertExpected();
    }
}
