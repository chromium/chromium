// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.metrics;

import android.content.ComponentName;
import android.content.Intent;

import androidx.test.filters.LargeTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.InOrder;
import org.mockito.Mockito;
import org.mockito.Spy;

import org.chromium.base.ActivityState;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.transit.TravelException;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DisableIf;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.customtabs.CustomTabActivity;
import org.chromium.chrome.browser.customtabs.CustomTabActivityTestRule;
import org.chromium.chrome.browser.customtabs.CustomTabsIntentTestUtils;
import org.chromium.chrome.browser.flags.ActivityType;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.settings.SettingsInTab;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.chrome.test.transit.ChromeTriggers;
import org.chromium.chrome.test.transit.FreshCtaTransitTestRule;
import org.chromium.chrome.test.transit.page.WebPageStation;
import org.chromium.chrome.test.transit.settings.SettingsStation;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.test.util.DeviceRestriction;

/** Public Transit tests for the UmaActivityObserver. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@DoNotBatch(reason = "Batching not yet supported in multi-window")
// In phones, the New Window option in the app menu is only enabled when already in multi-window or
// multi-display mode with Chrome not running in an adjacent window.
@Restriction({DeviceFormFactor.TABLET_OR_DESKTOP, DeviceRestriction.RESTRICTION_TYPE_NON_AUTO})
@EnableFeatures({ChromeFeatureList.UMA_SESSION_CORRECTNESS_FIXES})
@DisableIf.Device(DeviceFormFactor.DESKTOP_FREEFORM) // crbug.com/511287906
public class UmaActivityObserverTest {
    @Rule
    public FreshCtaTransitTestRule mCtaTestRule =
            ChromeTransitTestRules.freshChromeTabbedActivityRule();

    @Rule public CustomTabActivityTestRule mCctTestRule = new CustomTabActivityTestRule();

    @Spy private UmaSessionStats.Natives mUmaSessionStatsJniSpy;
    InOrder mInOrder;

    @Before
    public void setUp() {
        mUmaSessionStatsJniSpy = Mockito.spy(UmaSessionStatsJni.get());
        UmaSessionStatsJni.setInstanceForTesting(mUmaSessionStatsJniSpy);
        mInOrder = Mockito.inOrder(mUmaSessionStatsJniSpy);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Assert.assertEquals(
                            ActivityType.PRE_FIRST_TAB,
                            UmaActivityObserver.getCurrentActivityType());
                });
    }

    @Test
    @LargeTest
    public void testMultiWindowMetrics() throws Exception {
        WebPageStation pageInFirstWindow = mCtaTestRule.startOnBlankPage();

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Assert.assertEquals(
                            ActivityType.TABBED, UmaActivityObserver.getCurrentActivityType());
                });
        mInOrder.verify(mUmaSessionStatsJniSpy, Mockito.times(1)).umaResumeSession();
        mInOrder.verify(mUmaSessionStatsJniSpy, Mockito.times(0)).umaEndSession();

        // Create second CTA window. This should be unnecessary but I cannot figure out how to
        // convince Android to create an adjacent window for a new Activity. In any case, this
        // is useful to ensure the session isn't restarted.
        try {
            pageInFirstWindow.openRegularTabAppMenu().openNewWindow();
        } catch (TravelException e) {
            // On android_32_google_apis_x64_foldable the screen size is too small for the search
            // box to become fully visible with two windows. We don't care as we don't interact with
            // it.
            if (!e.getMessage().contains("id/search_box")) throw e;
        }
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Assert.assertEquals(
                            ActivityType.TABBED, UmaActivityObserver.getCurrentActivityType());
                });
        // Resume needs to happen before end to ensure the log isn't closed.
        mInOrder.verify(mUmaSessionStatsJniSpy, Mockito.times(1)).umaResumeSession();
        mInOrder.verify(mUmaSessionStatsJniSpy, Mockito.times(1)).umaEndSession();

        // Start a CCT over the second window.
        Intent intent =
                CustomTabsIntentTestUtils.createMinimalCustomTabIntent(
                        mCtaTestRule.getActivity(),
                        mCtaTestRule
                                .getTestServer()
                                .getURL("/chrome/test/data/android/about.html"));
        intent.addFlags(Intent.FLAG_ACTIVITY_LAUNCH_ADJACENT);
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        intent.setComponent(new ComponentName(mCtaTestRule.getActivity(), CustomTabActivity.class));

        mCctTestRule.startCustomTabActivityWithIntent(intent);
        CriteriaHelper.pollUiThread(
                () -> UmaActivityObserver.getCurrentActivityType() == ActivityType.CUSTOM_TAB);
        mInOrder.verify(mUmaSessionStatsJniSpy, Mockito.times(1)).umaEndSession();
        mInOrder.verify(mUmaSessionStatsJniSpy, Mockito.times(1)).umaResumeSession();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Assert.assertEquals(
                            ActivityState.RESUMED,
                            ApplicationStatus.getStateForActivity(mCtaTestRule.getActivity()));
                    Assert.assertEquals(
                            ActivityState.RESUMED,
                            ApplicationStatus.getStateForActivity(mCctTestRule.getActivity()));
                });

        // Focus existing CTA in first window.
        Intent intent2 = mCtaTestRule.getActivity().getIntent();
        intent2.addFlags(Intent.FLAG_ACTIVITY_LAUNCH_ADJACENT);
        intent2.addFlags(Intent.FLAG_ACTIVITY_SINGLE_TOP);
        mCctTestRule.getActivity().startActivity(intent2);

        CriteriaHelper.pollUiThread(
                () -> UmaActivityObserver.getCurrentActivityType() == ActivityType.TABBED);
        mInOrder.verify(mUmaSessionStatsJniSpy, Mockito.times(1)).umaEndSession();
        mInOrder.verify(mUmaSessionStatsJniSpy, Mockito.times(1)).umaResumeSession();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Assert.assertEquals(
                            ActivityState.RESUMED,
                            ApplicationStatus.getStateForActivity(mCtaTestRule.getActivity()));
                    Assert.assertEquals(
                            ActivityState.RESUMED,
                            ApplicationStatus.getStateForActivity(mCctTestRule.getActivity()));
                });

        // Close the CTA, resuming the CCT session.
        mCtaTestRule.getActivity().finish();

        CriteriaHelper.pollUiThread(
                () -> UmaActivityObserver.getCurrentActivityType() == ActivityType.CUSTOM_TAB);
        mInOrder.verify(mUmaSessionStatsJniSpy, Mockito.times(1)).umaEndSession();
        mInOrder.verify(mUmaSessionStatsJniSpy, Mockito.times(1)).umaResumeSession();
    }

    @Test
    @LargeTest
    public void testSessionPreservedInSettings() throws Exception {
        // When settings are shown in a tab, the settings "activity" is the tabbed activity itself.
        boolean settingsInTab = SettingsInTab.shouldOpenSettingsInTab();
        WebPageStation pageInFirstWindow = mCtaTestRule.startOnBlankPage();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Assert.assertEquals(
                            ActivityType.TABBED, UmaActivityObserver.getCurrentActivityType());
                });
        mInOrder.verify(mUmaSessionStatsJniSpy, Mockito.times(1)).umaResumeSession();
        mInOrder.verify(mUmaSessionStatsJniSpy, Mockito.times(0)).umaEndSession();

        // Open settings, preserving the session.
        SettingsStation settingsStation = pageInFirstWindow.openRegularTabAppMenu().openSettings();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Assert.assertEquals(
                            ActivityType.TABBED, UmaActivityObserver.getCurrentActivityType());
                });
        mInOrder.verify(mUmaSessionStatsJniSpy, Mockito.times(0)).umaResumeSession();
        mInOrder.verify(mUmaSessionStatsJniSpy, Mockito.times(0)).umaEndSession();

        // Start a non-chrome activity (file chooser) for result so we can close it.
        int requestCode = 1234;
        Intent intent = new Intent(Intent.ACTION_OPEN_DOCUMENT);
        intent.setType("image/*");
        intent.addCategory(Intent.CATEGORY_OPENABLE);
        try {
            settingsStation.getActivity().startActivityForResult(intent, 1234);
            CriteriaHelper.pollUiThread(
                    () ->
                            ApplicationStatus.getStateForActivity(settingsStation.getActivity())
                                    == ActivityState.STOPPED);
            mInOrder.verify(mUmaSessionStatsJniSpy, Mockito.times(0)).umaResumeSession();
            mInOrder.verify(mUmaSessionStatsJniSpy, Mockito.times(1)).umaEndSession();
        } finally {
            // Close the file chooser activity.
            settingsStation.getActivity().finishActivity(requestCode);
        }

        CriteriaHelper.pollUiThread(
                () ->
                        ApplicationStatus.getStateForActivity(settingsStation.getActivity())
                                == ActivityState.RESUMED);

        // With settings in a tab, the tabbed activity resumed, which restarts the session. With a
        // separate SettingsActivity, ideally we would re-start the session here, but this is
        // complicated and enough of an edge case that it's not worth fixing for now.
        mInOrder.verify(mUmaSessionStatsJniSpy, Mockito.times(settingsInTab ? 1 : 0))
                .umaResumeSession();
        mInOrder.verify(mUmaSessionStatsJniSpy, Mockito.times(0)).umaEndSession();

        // Close settings and return to Tabbed activity.
        if (settingsInTab) {
            ChromeTriggers.closeCurrentTabProgrammaticallyTo(
                            settingsStation, mCtaTestRule.getActivity())
                    .arriveAt(WebPageStation.newBuilder().initFrom(pageInFirstWindow).build());
        } else {
            settingsStation.getActivity().finish();
        }
        CriteriaHelper.pollUiThread(
                () ->
                        ApplicationStatus.getStateForActivity(mCtaTestRule.getActivity())
                                == ActivityState.RESUMED);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Assert.assertEquals(
                            ActivityType.TABBED, UmaActivityObserver.getCurrentActivityType());
                });
        mInOrder.verify(mUmaSessionStatsJniSpy, Mockito.times(settingsInTab ? 0 : 1))
                .umaResumeSession();
        mInOrder.verify(mUmaSessionStatsJniSpy, Mockito.times(0)).umaEndSession();
    }
}
