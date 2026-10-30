// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.memory_leaks;

import android.content.Context;
import android.content.Intent;
import android.os.Build;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewTreeObserver;

import androidx.appcompat.content.res.AppCompatResources;
import androidx.browser.customtabs.CustomTabsSessionToken;
import androidx.browser.customtabs.EngagementSignalsCallback;
import androidx.test.core.app.ApplicationProvider;
import androidx.test.filters.LargeTest;
import androidx.test.platform.app.InstrumentationRegistry;

import org.junit.After;
import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ActivityState;
import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.ServiceLoaderUtil;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.transit.TripBuilder;
import org.chromium.base.test.util.ApplicationTestUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DisableIf;
import org.chromium.base.test.util.EnableLeakChecks;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.ImportantFormFactors;
import org.chromium.base.test.util.MinAndroidSdkLevel;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.app.download.home.DownloadActivity;
import org.chromium.chrome.browser.app.download.home.DownloadActivityLauncher;
import org.chromium.chrome.browser.auxiliary_search.AuxiliarySearchControllerFactory;
import org.chromium.chrome.browser.auxiliary_search.AuxiliarySearchHooks;
import org.chromium.chrome.browser.customtabs.CustomTabsConnection;
import org.chromium.chrome.browser.customtabs.CustomTabsIntentTestUtils;
import org.chromium.chrome.browser.customtabs.CustomTabsTestUtils;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.privacy.settings.PrivacyPreferencesManagerImpl;
import org.chromium.chrome.browser.settings.SettingsInTab;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.transit.CctTransitTestRule;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.chrome.test.transit.ChromeTriggers;
import org.chromium.chrome.test.transit.FreshCtaTransitTestRule;
import org.chromium.chrome.test.transit.page.WebPageStation;
import org.chromium.chrome.test.transit.settings.SettingsStation;
import org.chromium.chrome.test.util.ActivityTestUtils;
import org.chromium.components.browser_ui.widget.R;
import org.chromium.components.browser_ui.widget.async_image.AsyncImageView;
import org.chromium.ui.base.DeviceFormFactor;

import java.util.concurrent.TimeoutException;
import java.util.concurrent.atomic.AtomicReference;

/** Tests the behavior of {@link ChromeFeatureList} in instrumentation tests. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@ImportantFormFactors(DeviceFormFactor.TABLET_OR_DESKTOP)
@Batch(Batch.PER_CLASS)
@EnableLeakChecks
public class PublicTransitLeakTest {
    @Rule
    public FreshCtaTransitTestRule mChromeTabbedActivityTestRule =
            ChromeTransitTestRules.freshChromeTabbedActivityRule();

    @Rule public CctTransitTestRule mCustomTabActivityTestRule = new CctTransitTestRule();

    @After
    public void tearDown() {
        mChromeTabbedActivityTestRule.closeAllWindowsAndDeleteInstanceAndTabState();
    }

    @Test
    @LargeTest
    public void basicChromeActivityTest() {
        mChromeTabbedActivityTestRule.startOnBlankPage();
    }

    @Test
    @LargeTest
    public void settingsActivityTest() {
        WebPageStation page = mChromeTabbedActivityTestRule.startOnBlankPage();
        SettingsStation settings = page.openRegularTabAppMenu().openSettings();
        // On desktop, back at the task root is handled by the system and does not close the
        // settings tab, so close it programmatically instead.
        TripBuilder exitSettings =
                SettingsInTab.shouldOpenSettingsInTab()
                        ? ChromeTriggers.closeCurrentTabProgrammaticallyTo(
                                settings, mChromeTabbedActivityTestRule.getActivity())
                        : settings.pressBackTo();
        exitSettings.arriveAt(WebPageStation.newBuilder().initFrom(page).build());
    }

    @Test
    @LargeTest
    @Restriction(DeviceFormFactor.PHONE)
    public void bookmarksActivityTest() {
        WebPageStation page = mChromeTabbedActivityTestRule.startOnBlankPage();
        page.openRegularTabAppMenu()
                .openBookmarksPhone()
                .pressBackTo()
                .arriveAt(WebPageStation.newBuilder().initFrom(page).build());
    }

    @Test
    @LargeTest
    // For some reason, this test is flaky on desktop.
    @DisableIf.Device(DeviceFormFactor.DESKTOP)
    public void searchActivityTest() {
        var page = mChromeTabbedActivityTestRule.startOnBlankPage();
        var activity = mChromeTabbedActivityTestRule.getActivity();
        page.openRegularTabSwitcher()
                .openTabSwitcherSearch()
                .pressBackToRegularTabSwitcher(activity);
    }

    @Test
    @LargeTest
    public void customTabActivityTest() throws TimeoutException {
        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(
                CustomTabsIntentTestUtils.createMinimalCustomTabIntent(
                        ApplicationProvider.getApplicationContext(),
                        mChromeTabbedActivityTestRule
                                .getTestServer()
                                .getURL("/chrome/test/data/android/google.html")));
    }

    @Test
    @LargeTest
    @MinAndroidSdkLevel(Build.VERSION_CODES.S)
    @EnableFeatures({ChromeFeatureList.ANDROID_APP_INTEGRATION_MULTI_DATA_SOURCE})
    public void chromeTabbedActivityWithAuxiliarySearchTest() {
        ServiceLoaderUtil.setInstanceForTesting(AuxiliarySearchHooks.class, () -> true);
        AuxiliarySearchControllerFactory.getInstance().setSupportMultiDataSourceForTesting(true);
        mChromeTabbedActivityTestRule.startOnBlankPage();
    }

    @Test
    @LargeTest
    @MinAndroidSdkLevel(Build.VERSION_CODES.S)
    @EnableFeatures({ChromeFeatureList.ANDROID_APP_INTEGRATION_MULTI_DATA_SOURCE})
    public void customTabActivityWithAuxiliarySearchTest() throws TimeoutException {
        ServiceLoaderUtil.setInstanceForTesting(AuxiliarySearchHooks.class, () -> true);
        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(
                CustomTabsIntentTestUtils.createMinimalCustomTabIntent(
                        ApplicationProvider.getApplicationContext(),
                        mChromeTabbedActivityTestRule
                                .getTestServer()
                                .getURL("/chrome/test/data/android/google.html")));
    }

    @Test
    @LargeTest
    public void customTabActivityWithSessionTest() throws TimeoutException {
        final CustomTabsConnection connection = CustomTabsTestUtils.warmUpAndWait();
        Context context = ApplicationProvider.getApplicationContext();
        Intent intent =
                CustomTabsIntentTestUtils.createMinimalCustomTabIntent(
                        context,
                        mChromeTabbedActivityTestRule
                                .getTestServer()
                                .getURL("/chrome/test/data/android/google.html"));
        final CustomTabsSessionToken token =
                CustomTabsSessionToken.getSessionTokenFromIntent(intent);
        Assert.assertTrue(connection.newSession(token));
        org.chromium.base.ThreadUtils.runOnUiThreadBlocking(
                () -> PrivacyPreferencesManagerImpl.getInstance().setUsageAndCrashReporting(true));
        Assert.assertTrue(
                connection.setEngagementSignalsCallback(
                        token, new EngagementSignalsCallback() {}, android.os.Bundle.EMPTY));

        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(intent);
    }

    @Test
    @LargeTest
    public void omniboxTest() {
        var page = mChromeTabbedActivityTestRule.startOnBlankPage();
        var omnibox = page.openOmnibox();
        omnibox.pressBackTo().exitFacility();
    }

    @Test
    @LargeTest
    public void ntpAndIncognitoNtpTest() {
        var page = mChromeTabbedActivityTestRule.startOnBlankPage();
        var ntp = page.openNewTabFast();
        ntp.openNewIncognitoTabOrWindowFast();
    }

    @Test
    @LargeTest
    @DisableIf.Device(DeviceFormFactor.DESKTOP)
    public void customTabActivityAfterChromeTabbedActivityTest() {
        Intent intent =
                CustomTabsIntentTestUtils.createMinimalCustomTabIntent(
                        ApplicationProvider.getApplicationContext(),
                        mChromeTabbedActivityTestRule
                                .getTestServer()
                                .getURL("/chrome/test/data/android/google.html"));
        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(intent);
        mChromeTabbedActivityTestRule.startOnBlankPage();
        var cta = mChromeTabbedActivityTestRule.getActivity();
        var cct = mCustomTabActivityTestRule.getActivity();
        CriteriaHelper.pollUiThread(
                () -> ApplicationStatus.getStateForActivity(cct) == ActivityState.STOPPED);
        ThreadUtils.runOnUiThreadBlocking(
                () -> ApiCompatibilityUtils.moveTaskToFront(cta, cct.getTaskId(), 0));
        CriteriaHelper.pollUiThread(
                () ->
                        ApplicationStatus.getStateForActivity(cct) == ActivityState.RESUMED
                                && ApplicationStatus.getStateForActivity(cta)
                                        == ActivityState.STOPPED);
        mChromeTabbedActivityTestRule.finishActivity();
    }

    @Test
    @LargeTest
    public void downloadActivityWithAsyncImageViewTest() throws Exception {
        mChromeTabbedActivityTestRule.startOnBlankPage();
        var cta = mChromeTabbedActivityTestRule.getActivity();
        var downloadActivity =
                ActivityTestUtils.waitForActivity(
                        InstrumentationRegistry.getInstrumentation(),
                        DownloadActivity.class,
                        () ->
                                ThreadUtils.runOnUiThreadBlocking(
                                        () ->
                                                DownloadActivityLauncher.getInstance()
                                                        .showDownloadActivity(cta, null, false)));
        try {
            // Attach an AsyncImageView configured with an animated waiting drawable (wrapped by
            // AutoAnimatorDrawable) and a pending async image request so the waiting animation
            // starts and binds its RenderThread VectorDrawableAnimatorRT to DownloadActivity's
            // RenderNode on first draw. Then hide the view and finish DownloadActivity;
            // @EnableLeakChecks verifies after the test that stopping the hidden
            // AutoAnimatorDrawable releases the activity rather than keeping it alive via a
            // restarted animation callback.
            CallbackHelper firstDraw = new CallbackHelper();
            AtomicReference<AsyncImageView> viewRef = new AtomicReference<>();
            ThreadUtils.runOnUiThreadBlocking(
                    () -> {
                        AsyncImageView asyncImageView = new AsyncImageView(downloadActivity);
                        viewRef.set(asyncImageView);
                        asyncImageView.setWaitingDrawable(
                                AppCompatResources.getDrawable(
                                        downloadActivity, R.drawable.async_image_view_waiting));
                        ViewGroup content = downloadActivity.findViewById(android.R.id.content);
                        content.addView(asyncImageView, new ViewGroup.LayoutParams(100, 100));
                        asyncImageView.setAsyncImageDrawable((consumer, w, h) -> () -> {}, "test");
                        asyncImageView
                                .getViewTreeObserver()
                                .addOnDrawListener(
                                        new ViewTreeObserver.OnDrawListener() {
                                            @Override
                                            public void onDraw() {
                                                firstDraw.notifyCalled();
                                            }
                                        });
                    });
            firstDraw.waitForNext();
            ThreadUtils.runOnUiThreadBlocking(() -> viewRef.get().setVisibility(View.GONE));
        } finally {
            ApplicationTestUtils.finishActivity(downloadActivity);
        }
    }
}
