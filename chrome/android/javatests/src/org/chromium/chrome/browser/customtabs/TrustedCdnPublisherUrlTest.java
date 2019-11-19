// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;

import android.app.Activity;
import android.app.Instrumentation;
import android.content.Context;
import android.content.Intent;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.graphics.drawable.BitmapDrawable;
import android.support.test.InstrumentationRegistry;
import android.support.test.filters.SmallTest;
import android.util.Pair;
import android.view.View;
import android.widget.ImageView;

import androidx.annotation.Nullable;
import androidx.browser.customtabs.CustomTabsIntent;
import androidx.browser.customtabs.CustomTabsSessionToken;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.CommandLine;
import org.chromium.base.library_loader.LibraryLoader;
import org.chromium.base.library_loader.LibraryProcessType;
import org.chromium.base.task.PostTask;
import org.chromium.base.test.util.AnnotationRule;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.firstrun.FirstRunStatus;
import org.chromium.chrome.browser.offlinepages.ClientId;
import org.chromium.chrome.browser.offlinepages.OfflinePageBridge;
import org.chromium.chrome.browser.omnibox.UrlBar;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TrustedCdn;
import org.chromium.chrome.browser.test.ScreenShooter;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.components.offlinepages.SavePageResult;
import org.chromium.components.url_formatter.UrlFormatter;
import org.chromium.content_public.browser.UiThreadTaskTraits;
import org.chromium.content_public.browser.test.util.Criteria;
import org.chromium.content_public.browser.test.util.CriteriaHelper;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.content_public.browser.test.util.TestTouchUtils;
import org.chromium.net.NetworkChangeNotifier;
import org.chromium.net.test.util.TestWebServer;
import org.chromium.ui.base.DeviceFormFactor;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.Collections;
import java.util.List;
import java.util.Locale;
import java.util.concurrent.TimeoutException;

/**
 * Instrumentation tests for showing the publisher URL for a trusted CDN.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class TrustedCdnPublisherUrlTest {
    @Rule
    public CustomTabActivityTestRule mCustomTabActivityTestRule = new CustomTabActivityTestRule();

    private static final String PAGE_WITH_TITLE =
            "<!DOCTYPE html><html><head><title>Example title</title></head></html>";

    private TestWebServer mWebServer;

    @Rule
    public final ScreenShooter mScreenShooter = new ScreenShooter();

    /**
     * Annotation to override the trusted CDN.
     */
    @Retention(RetentionPolicy.RUNTIME)
    private @interface OverrideTrustedCdn {}

    private static class OverrideTrustedCdnRule extends AnnotationRule {
        public OverrideTrustedCdnRule() {
            super(OverrideTrustedCdn.class);
        }

        /**
         * @return Whether the trusted CDN should be overridden.
         */
        public boolean isEnabled() {
            return !getAnnotations().isEmpty();
        }
    }

    @Rule
    public OverrideTrustedCdnRule mOverrideTrustedCdn = new OverrideTrustedCdnRule();

    @Before
    public void setUp() throws Exception {
        TestThreadUtils.runOnUiThreadBlocking(() -> FirstRunStatus.setFirstRunFlowComplete(true));

        LibraryLoader.getInstance().ensureInitialized(LibraryProcessType.PROCESS_BROWSER);
        mWebServer = TestWebServer.start();
        if (mOverrideTrustedCdn.isEnabled()) {
            CommandLine.getInstance().appendSwitchWithValue(
                    "trusted-cdn-base-url-for-tests", mWebServer.getBaseUrl());
        }
    }

    @After
    public void tearDown() {
        TestThreadUtils.runOnUiThreadBlocking(() -> FirstRunStatus.setFirstRunFlowComplete(false));

        mWebServer.shutdown();
    }

    @Test
    //@SmallTest
    //@Feature({"UiCatalogue"})
    @Features.EnableFeatures(ChromeFeatureList.SHOW_TRUSTED_PUBLISHER_URL)
    @OverrideTrustedCdn
    @DisabledTest // Disabled for flakiness! See http://crbug.com/851950
    public void testHttps() throws Exception {
        runTrustedCdnPublisherUrlTest("https://www.example.com/test", "com.example.test",
                "example.com", org.chromium.chrome.R.drawable.omnibox_https_valid);
        mScreenShooter.shoot("trustedPublisherUrlHttps");
    }

    @Test
    @SmallTest
    @Feature({"UiCatalogue"})
    @Features.EnableFeatures(ChromeFeatureList.SHOW_TRUSTED_PUBLISHER_URL)
    @OverrideTrustedCdn
    @DisabledTest // Disabled for flakiness! See http://crbug.com/847341
    public void testHttp() throws Exception {
        runTrustedCdnPublisherUrlTest("http://example.com/test", "com.example.test", "example.com",
                org.chromium.chrome.R.drawable.omnibox_info);
        mScreenShooter.shoot("trustedPublisherUrlHttp");
    }

    @Test
    @SmallTest
    @Feature({"UiCatalogue"})
    @Features.EnableFeatures(ChromeFeatureList.SHOW_TRUSTED_PUBLISHER_URL)
    @OverrideTrustedCdn
    @DisabledTest // Disabled for flakiness! See http://crbug.com/847341
    public void testRtl() throws Exception {
        String publisher = "\u200e\u202b\u0645\u0648\u0642\u0639\u002e\u0648\u0632\u0627\u0631"
                + "\u0629\u002d\u0627\u0644\u0623\u062a\u0635\u0627\u0644\u0627\u062a\u002e\u0645"
                + "\u0635\u0631\u202c\u200e";
        runTrustedCdnPublisherUrlTest("http://xn--4gbrim.xn----rmckbbajlc6dj7bxne2c.xn--wgbh1c/",
                "com.example.test", publisher, org.chromium.chrome.R.drawable.omnibox_info);
        mScreenShooter.shoot("trustedPublisherUrlRtl");
    }

    private int getDefaultSecurityIcon() {
        // On tablets an info icon is shown for ConnectionSecurityLevel.NONE pages,
        // on smaller form factors nothing.
        if (DeviceFormFactor.isNonMultiDisplayContextOnTablet(
                    InstrumentationRegistry.getTargetContext())) {
            return org.chromium.chrome.R.drawable.omnibox_info;
        }

        return 0;
    }

    @Test
    @SmallTest
    @Feature({"UiCatalogue"})
    @Features.EnableFeatures(ChromeFeatureList.SHOW_TRUSTED_PUBLISHER_URL)
    @OverrideTrustedCdn
    public void testUntrustedClient() throws Exception {
        runTrustedCdnPublisherUrlTest(
                "https://example.com/test", "com.someoneelse.bla", null, getDefaultSecurityIcon());
    }

    @Test
    @SmallTest
    @Feature({"UiCatalogue"})
    @Features.EnableFeatures(ChromeFeatureList.SHOW_TRUSTED_PUBLISHER_URL)
    @OverrideTrustedCdn
    public void testNoHeader() throws Exception {
        runTrustedCdnPublisherUrlTest(null, "com.example.test", null, getDefaultSecurityIcon());
    }

    @Test
    @SmallTest
    @Feature({"UiCatalogue"})
    @Features.EnableFeatures(ChromeFeatureList.SHOW_TRUSTED_PUBLISHER_URL)
    @OverrideTrustedCdn
    public void testMalformedHeader() throws Exception {
        runTrustedCdnPublisherUrlTest(
                "garbage", "com.example.test", null, getDefaultSecurityIcon());
    }

    @Test
    @SmallTest
    @Feature({"UiCatalogue"})
    @Features.DisableFeatures(ChromeFeatureList.SHOW_TRUSTED_PUBLISHER_URL)
    @OverrideTrustedCdn
    public void testDisabled() throws Exception {
        runTrustedCdnPublisherUrlTest(
                "https://example.com/test", "com.example.test", null, getDefaultSecurityIcon());
    }

    @Test
    @SmallTest
    @Feature({"UiCatalogue"})
    @Features.EnableFeatures(ChromeFeatureList.SHOW_TRUSTED_PUBLISHER_URL)
    // No @OverrideTrustedCdn
    public void testUntrustedCdn() throws Exception {
        runTrustedCdnPublisherUrlTest(
                "https://example.com/test", "com.example.test", null, getDefaultSecurityIcon());
    }

    @Test
    @SmallTest
    @Feature({"UiCatalogue"})
    @Features.EnableFeatures(ChromeFeatureList.SHOW_TRUSTED_PUBLISHER_URL)
    @OverrideTrustedCdn
    @DisabledTest // Disabled for flakiness! See http://crbug.com/847341
    public void testPageInfo() throws Exception {
        runTrustedCdnPublisherUrlTest("https://example.com/test", "com.example.test", "example.com",
                R.drawable.omnibox_https_valid);
        TestTouchUtils.performClickOnMainSync(InstrumentationRegistry.getInstrumentation(),
                mCustomTabActivityTestRule.getActivity().findViewById(R.id.security_button));
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();
        mScreenShooter.shoot("Page Info");
    }

    // TODO(bauerb): Test an insecure HTTPS connection.

    @Test
    @SmallTest
    @Feature({"UiCatalogue"})
    @Features.EnableFeatures(ChromeFeatureList.SHOW_TRUSTED_PUBLISHER_URL)
    @OverrideTrustedCdn
    @DisabledTest // Disabled for flakiness! See http://crbug.com/847341
    public void testNavigateAway() throws Exception {
        runTrustedCdnPublisherUrlTest("https://example.com/test", "com.example.test", "example.com",
                R.drawable.omnibox_https_valid);

        String otherTestUrl = mWebServer.setResponse("/other.html", PAGE_WITH_TITLE, null);
        mCustomTabActivityTestRule.loadUrl(otherTestUrl);

        verifyUrl(UrlFormatter.formatUrlForSecurityDisplayOmitScheme(otherTestUrl));
        // TODO(bauerb): The security icon is updated via an animation. Find a way to reliably
        // disable animations and verify the icon.
    }

    @Test
    @SmallTest
    @Feature({"UiCatalogue"})
    @Features.EnableFeatures(ChromeFeatureList.SHOW_TRUSTED_PUBLISHER_URL)
    @OverrideTrustedCdn
    @DisabledTest // Disabled for flakiness! See http://crbug.com/847341
    public void testReparent() throws Exception {
        String publisherUrl = "https://example.com/test";
        runTrustedCdnPublisherUrlTest(
                publisherUrl, "com.example.test", "example.com", R.drawable.omnibox_https_valid);

        final Instrumentation.ActivityMonitor monitor =
                InstrumentationRegistry.getInstrumentation().addMonitor(
                        ChromeTabbedActivity.class.getName(), /* result = */ null, false);
        CustomTabActivity customTabActivity = mCustomTabActivityTestRule.getActivity();
        final Tab tab = customTabActivity.getActivityTab();
        PostTask.postTask(UiThreadTaskTraits.DEFAULT, () -> {
            Assert.assertEquals(publisherUrl, TrustedCdn.getPublisherUrl(tab));
            customTabActivity.getComponent().resolveNavigationController()
                    .openCurrentUrlInBrowser(true);
            Assert.assertNull(customTabActivity.getActivityTab());
        });

        // Use the extended CriteriaHelper timeout to make sure we get an activity
        final Activity activity =
                monitor.waitForActivityWithTimeout(CriteriaHelper.DEFAULT_MAX_TIME_TO_POLL);
        Assert.assertNotNull(
                "Monitor did not get an activity before hitting the timeout", activity);
        Assert.assertTrue(
                "Expected activity to be a ChromeActivity, was " + activity.getClass().getName(),
                activity instanceof ChromeActivity);
        final ChromeActivity newActivity = (ChromeActivity) activity;
        CriteriaHelper.pollUiThread(() -> newActivity.getActivityTab() == tab, "Tab did not load");

        TestThreadUtils.runOnUiThreadBlocking(
                () -> { Assert.assertNull(TrustedCdn.getPublisherUrl(tab)); });

        String testUrl = mWebServer.getResponseUrl("/test.html");
        String expectedUrl = UrlFormatter.formatUrlForDisplayOmitScheme(testUrl);

        CriteriaHelper.pollUiThread(Criteria.equals(expectedUrl, () -> {
            UrlBar urlBar = newActivity.findViewById(R.id.url_bar);
            return urlBar.getText().toString();
        }));

        verifySecurityIcon(getDefaultSecurityIcon());
    }

    @Test
    @SmallTest
    @Features.EnableFeatures(ChromeFeatureList.SHOW_TRUSTED_PUBLISHER_URL)
    @OverrideTrustedCdn
    @DisabledTest // Disabled for flakiness! See http://crbug.com/847341
    public void testOfflinePage() throws TimeoutException {
        String publisherUrl = "https://example.com/test";
        runTrustedCdnPublisherUrlTest(
                publisherUrl, "com.example.test", "example.com", R.drawable.omnibox_https_valid);

        OfflinePageBridge offlinePageBridge =
                TestThreadUtils.runOnUiThreadBlockingNoException(() -> {
                    Profile profile = Profile.getLastUsedProfile();
                    return OfflinePageBridge.getForProfile(profile);
                });

        // Wait until the offline page model has been loaded.
        CallbackHelper callback = new CallbackHelper();
        PostTask.runOrPostTask(UiThreadTaskTraits.DEFAULT, () -> {
            if (offlinePageBridge.isOfflinePageModelLoaded()) {
                callback.notifyCalled();
                return;
            }
            offlinePageBridge.addObserver(new OfflinePageBridge.OfflinePageModelObserver() {
                @Override
                public void offlinePageModelLoaded() {
                    callback.notifyCalled();
                    offlinePageBridge.removeObserver(this);
                }
            });
        });
        callback.waitForCallback(0);

        CallbackHelper callback2 = new CallbackHelper();
        PostTask.runOrPostTask(UiThreadTaskTraits.DEFAULT, () -> {
            CustomTabActivity customTabActivity = mCustomTabActivityTestRule.getActivity();
            Tab tab = customTabActivity.getActivityTab();
            String pageUrl = tab.getUrl();
            offlinePageBridge.savePage(tab.getWebContents(),
                    new ClientId(OfflinePageBridge.DOWNLOAD_NAMESPACE, "1234"),
                    (savePageResult, url, offlineId) -> {
                        Assert.assertEquals(SavePageResult.SUCCESS, savePageResult);
                        Assert.assertEquals(pageUrl, url);
                        // offlineId
                        callback2.notifyCalled();
                    });
        });
        callback2.waitForCallback(0);

        TestThreadUtils.runOnUiThreadBlocking(
                () -> { NetworkChangeNotifier.forceConnectivityState(false); });

        // Load the URL in the same tab. With no connectivity, loading the offline page should
        // succeed, but not show a publisher URL.
        String testUrl = mWebServer.getResponseUrl("/test.html");
        mCustomTabActivityTestRule.loadUrl(testUrl);
        verifyUrl(UrlFormatter.formatUrlForSecurityDisplayOmitScheme(testUrl));
        verifySecurityIcon(R.drawable.ic_offline_pin_24dp);
    }

    private void runTrustedCdnPublisherUrlTest(@Nullable String publisherUrl, String clientPackage,
            @Nullable String expectedPublisher, int expectedSecurityIcon) throws TimeoutException {
        final List<Pair<String, String>> headers;
        if (publisherUrl != null) {
            headers = Collections.singletonList(Pair.create("X-AMP-Cache", publisherUrl));
        } else {
            headers = null;
        }
        String testUrl = mWebServer.setResponse("/test.html", PAGE_WITH_TITLE, headers);
        Context targetContext = InstrumentationRegistry.getTargetContext();
        Intent intent = CustomTabsTestUtils.createMinimalCustomTabIntent(targetContext, testUrl);
        intent.putExtra(
                CustomTabsIntent.EXTRA_TITLE_VISIBILITY_STATE, CustomTabsIntent.SHOW_PAGE_TITLE);
        CustomTabsSessionToken token = CustomTabsSessionToken.getSessionTokenFromIntent(intent);
        CustomTabsConnection connection = CustomTabsTestUtils.warmUpAndWait();
        connection.newSession(token);
        connection.overridePackageNameForSessionForTesting(token, clientPackage);
        connection.setTrustedPublisherUrlPackageForTest("com.example.test");

        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(intent);

        final String expectedUrl;
        if (expectedPublisher == null) {
            expectedUrl = UrlFormatter.formatUrlForSecurityDisplayOmitScheme(testUrl);
        } else {
            expectedUrl =
                    String.format(Locale.US, "From %s – delivered by Google", expectedPublisher);
        }
        verifyUrl(expectedUrl);
        verifySecurityIcon(expectedSecurityIcon);
    }

    private void verifyUrl(String expectedUrl) {
        UrlBar urlBar = mCustomTabActivityTestRule.getActivity().findViewById(R.id.url_bar);
        Assert.assertEquals(expectedUrl, urlBar.getText().toString());
    }

    private void verifySecurityIcon(int expectedSecurityIcon) {
        ImageView securityButton =
                mCustomTabActivityTestRule.getActivity().findViewById(R.id.security_button);
        if (expectedSecurityIcon == 0) {
            Assert.assertEquals(View.INVISIBLE, securityButton.getVisibility());
        } else {
            Assert.assertEquals(View.VISIBLE, securityButton.getVisibility());
            Bitmap expectedIcon = BitmapFactory.decodeResource(
                    InstrumentationRegistry.getTargetContext().getResources(),
                    expectedSecurityIcon);
            Assert.assertTrue(expectedIcon.sameAs(
                    ((BitmapDrawable) securityButton.getDrawable()).getBitmap()));
        }
    }
}
