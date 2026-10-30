// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps;

import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.net.Uri;
import android.provider.Browser;

import androidx.test.core.app.ApplicationProvider;
import androidx.test.filters.LargeTest;

import org.hamcrest.Matchers;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.RuleChain;
import org.junit.runner.RunWith;

import org.chromium.base.IntentUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Features;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.blink.mojom.DisplayMode;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.banners.AppMenuVerbiage;
import org.chromium.chrome.browser.browserservices.TrustedWebActivityTestUtil;
import org.chromium.chrome.browser.customtabs.CustomTabActivity;
import org.chromium.chrome.browser.flags.ActivityType;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.test.MockCertVerifierRuleAndroid;
import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.components.webapps.AddToHomescreenCoordinator;
import org.chromium.components.webapps.TwaInstaller;
import org.chromium.components.webapps.WebappsUtils;
import org.chromium.content_public.browser.test.util.JavaScriptUtils;
import org.chromium.net.test.EmbeddedTestServerRule;

import java.util.concurrent.TimeoutException;
import java.util.concurrent.atomic.AtomicReference;

/** Integration tests for Auto-Minted TWA installation via {@link TwaInstaller}. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@DoNotBatch(reason = "Tests activity start behavior")
public class TwaInstallerIntegrationTest {
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();
    public EmbeddedTestServerRule mEmbeddedTestServerRule = new EmbeddedTestServerRule();
    public MockCertVerifierRuleAndroid mCertVerifierRule =
            new MockCertVerifierRuleAndroid(0 /* net::OK */);

    @Rule
    public RuleChain mRuleChain =
            RuleChain.emptyRuleChain()
                    .around(mActivityTestRule)
                    .around(mEmbeddedTestServerRule)
                    .around(mCertVerifierRule);

    private static final long STARTUP_TIMEOUT = 15000L;

    @Before
    public void setUp() {
        mEmbeddedTestServerRule.setServerUsesHttps(true);
    }

    /**
     * End-to-end integration test verifying that when {@code kAndroidAutoMintedTWA} is enabled, an
     * installable web app is installed via {@link TwaInstaller} and launched into a standalone
     * {@link CustomTabActivity} in Trusted Web Activity mode, matching {@code
     * window.matchMedia('(display-mode: standalone)').matches === true}.
     */
    @Test
    @LargeTest
    @Feature({"Webapps"})
    @Features.EnableFeatures({ChromeFeatureList.ANDROID_AUTO_MINTED_TWA})
    public void testInstallAndLaunchAutoMintedTwa() throws Exception {
        WebappsUtils.setIsWebAppServiceEnabledForTesting(true);

        String pageUrl =
                mEmbeddedTestServerRule
                        .getServer()
                        .getURL("/chrome/test/data/banners/manifest_test_page.html");
        String expectedManifestUrl =
                mEmbeddedTestServerRule
                        .getServer()
                        .getURL("/chrome/test/data/banners/manifest.json");

        ChromeTabbedActivity.interceptMoveTaskToBackForTesting();

        Context context = ApplicationProvider.getApplicationContext();
        Intent browserIntent = new Intent(Intent.ACTION_VIEW, Uri.parse("about:blank"));
        browserIntent.setComponent(new ComponentName(context, ChromeTabbedActivity.class));
        browserIntent.putExtra(Browser.EXTRA_APPLICATION_ID, context.getPackageName());
        browserIntent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        IntentUtils.addTrustedIntentExtras(browserIntent);
        context.startActivity(browserIntent);

        ChromeTabbedActivity tabbedActivity =
                ChromeActivityTestRule.waitFor(ChromeTabbedActivity.class, STARTUP_TIMEOUT);
        ChromeActivityTestRule.waitForActivityCompletelyLoaded(tabbedActivity);
        Tab tab = ThreadUtils.runOnUiThreadBlocking(() -> tabbedActivity.getActivityTab());
        ChromeTabUtils.loadUrlOnUiThread(tab, pageUrl);
        ChromeTabUtils.waitForTabPageLoaded(tab, pageUrl);

        Intent twaIntent =
                TrustedWebActivityTestUtil.createTrustedWebActivityIntentAndVerifiedSession(
                        pageUrl, context.getPackageName());
        twaIntent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);

        CallbackHelper installCallbackHelper = new CallbackHelper();
        AtomicReference<String> installedTitle = new AtomicReference<>();
        AtomicReference<String> installedManifestUrl = new AtomicReference<>();
        TwaInstaller.setInstallDelegateForTesting(
                (title,
                        manifestUrl,
                        installSucceededCallback,
                        installFailedCallback,
                        installCancelledCallback) -> {
                    installedTitle.set(title);
                    installedManifestUrl.set(manifestUrl);
                    installSucceededCallback.run();
                    context.startActivity(twaIntent);
                    installCallbackHelper.notifyCalled();
                    return true;
                });

        HistogramWatcher displayModeWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Webapp.Install.DisplayMode2", DisplayMode.STANDALONE);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    boolean started =
                            new AddToHomescreenCoordinator(
                                            tab.getWebContents(),
                                            tabbedActivity,
                                            tabbedActivity.getWindowAndroid(),
                                            tabbedActivity.getModalDialogManager())
                                    .showForAppMenu(AppMenuVerbiage.APP_MENU_OPTION_INSTALL);
                    Assert.assertTrue("AddToHomescreenCoordinator failed to start", started);
                });

        installCallbackHelper.waitForOnly();
        displayModeWatcher.pollInstrumentationThreadUntilSatisfied();

        Assert.assertEquals("Manifest test app", installedTitle.get());
        Assert.assertEquals(expectedManifestUrl, installedManifestUrl.get());

        CustomTabActivity customTabActivity =
                ChromeActivityTestRule.waitFor(CustomTabActivity.class, STARTUP_TIMEOUT);
        Assert.assertEquals(ActivityType.TRUSTED_WEB_ACTIVITY, customTabActivity.getActivityType());

        CriteriaHelper.pollUiThread(
                () -> {
                    Tab candidateTab = customTabActivity.getActivityTab();
                    Criteria.checkThat(candidateTab, Matchers.notNullValue());
                    Criteria.checkThat(candidateTab.getWebContents(), Matchers.notNullValue());
                },
                STARTUP_TIMEOUT,
                CriteriaHelper.DEFAULT_POLLING_INTERVAL);
        Tab twaTab = ThreadUtils.runOnUiThreadBlocking(() -> customTabActivity.getActivityTab());
        ChromeTabUtils.waitForTabPageLoaded(twaTab, pageUrl);
        Assert.assertTrue(TrustedWebActivityTestUtil.isTrustedWebActivity(customTabActivity));

        CriteriaHelper.pollInstrumentationThread(
                () -> {
                    try {
                        String result =
                                JavaScriptUtils.executeJavaScriptAndWaitForResult(
                                        twaTab.getWebContents(),
                                        "window.matchMedia('(display-mode: standalone)').matches");
                        Criteria.checkThat(result, Matchers.is("true"));
                    } catch (TimeoutException e) {
                        Criteria.checkThat("JS evaluation timed out", false, Matchers.is(true));
                    }
                },
                STARTUP_TIMEOUT,
                CriteriaHelper.DEFAULT_POLLING_INTERVAL);

        String displayModeMatches =
                JavaScriptUtils.executeJavaScriptAndWaitForResult(
                        twaTab.getWebContents(),
                        "window.matchMedia('(display-mode: standalone)').matches");
        Assert.assertEquals("true", displayModeMatches);

        int remainingTabCount =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> tabbedActivity.getTabModelSelector().getTotalTabCount());
        Assert.assertTrue(
                "Originating browser window should not remain with zero tabs",
                remainingTabCount > 0);
    }
}
