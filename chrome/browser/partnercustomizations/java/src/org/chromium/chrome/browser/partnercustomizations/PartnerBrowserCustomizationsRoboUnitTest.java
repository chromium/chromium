// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.partnercustomizations;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.RobolectricUtil;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.partnercustomizations.PartnerCustomizationsTestUtils.HomepageCharacterizationHelperStub;
import org.chromium.chrome.browser.partnercustomizations.PartnerCustomizationsUma.PartnerCustomizationsHomepageEnum;
import org.chromium.url.JUnitTestGURLs;

/** Unit tests for {@link PartnerBrowserCustomizations}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@EnableFeatures(ChromeFeatureList.PARTNER_CUSTOMIZATIONS_UMA)
public class PartnerBrowserCustomizationsRoboUnitTest {
    @Mock private ActivityLifecycleDispatcher mActivityLifecycleDispatcherMock;

    @Before
    public void setup() {
        CustomizationProviderDelegateUpstreamImpl.setHomepageForTesting(
                JUnitTestGURLs.EXAMPLE_URL.getSpec());

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
        RobolectricUtil.runOneBackgroundTask();
        assertFalse(
                "The homepage refreshed, but result is not yet posted on UI thread.",
                PartnerBrowserCustomizations.getInstance().isInitialized());

        // Assuming homepage is changed during 1st #initializeAsync, and another #initializeAsync is
        // triggered. The 2nd #initializeAsync is ignored since there's one already in the process.
        CustomizationProviderDelegateUpstreamImpl.setHomepageForTesting(
                JUnitTestGURLs.GOOGLE_URL.getSpec());
        PartnerBrowserCustomizations.getInstance()
                .initializeAsync(ContextUtils.getApplicationContext());
        assertFalse(
                "#initializeAsync should be in progress.",
                PartnerBrowserCustomizations.getInstance().isInitialized());

        RobolectricUtil.runAllBackgroundAndUi();
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

        RobolectricUtil.runAllBackgroundAndUi();
        assertTrue(
                "3rd #initializeAsync should be done.",
                PartnerBrowserCustomizations.getInstance().isInitialized());
        assertEquals(
                "Homepage should refreshed by 3rd #initializeAsync.",
                JUnitTestGURLs.GOOGLE_URL.getSpec(),
                PartnerBrowserCustomizations.getInstance().getHomePageUrl().getSpec());
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
        CustomizationProviderDelegateUpstreamImpl.setHomepageForTesting(null);
        HistogramWatcher histograms =
                HistogramWatcher.newSingleRecordWatcher(
                        "Android.PartnerCustomization.HomepageCustomizationOutcome",
                        PartnerCustomizationsHomepageEnum.NTP_CORRECTLY);

        // Setup the Async task and get it running.
        PartnerBrowserCustomizations.getInstance()
                .initializeAsync(ContextUtils.getApplicationContext());
        // Run one task, so AsyncTask#doInBackground is run, but not #onFinalized.
        RobolectricUtil.runOneBackgroundTask();

        // Simulate CTA#createInitialTab: Create an NTP when the delegate says Partner Homepage.
        // TODO(donnd): call this as an async callback through setOnInitializeAsyncFinished.
        PartnerBrowserCustomizations.getInstance()
                .onCreateInitialTab(
                        JUnitTestGURLs.NTP_NATIVE_URL.getSpec(),
                        mActivityLifecycleDispatcherMock,
                        HomepageCharacterizationHelperStub::ntpHelper);

        // Trigger Async completion.
        RobolectricUtil.runAllBackgroundAndUi();

        // Make sure the Outcome logged to UMA is correct.
        histograms.assertExpected();
    }
}
