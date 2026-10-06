// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.history;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;

import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.text.TextUtils;

import androidx.test.filters.MediumTest;

import org.hamcrest.Matchers;
import org.junit.AfterClass;
import org.junit.Before;
import org.junit.BeforeClass;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TemporaryFolder;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.params.ParameterAnnotations.UseMethodParameter;
import org.chromium.base.test.params.ParameterAnnotations.UseMethodParameterBefore;
import org.chromium.base.test.params.ParameterAnnotations.UseRunnerDelegate;
import org.chromium.base.test.params.ParameterizedRunner;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.CriteriaNotSatisfiedException;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.ViewportTestUtils;
import org.chromium.chrome.browser.browsing_data.BrowsingDataBridge;
import org.chromium.chrome.browser.browsing_data.BrowsingDataType;
import org.chromium.chrome.browser.browsing_data.TimePeriod;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.night_mode.ChromeNightModeTestUtils;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeJUnit4RunnerDelegate;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.chrome.test.transit.FreshCtaTransitTestRule;
import org.chromium.chrome.test.util.ChromeRenderTestRule;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.content_public.browser.RenderWidgetHostView;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.test.util.JavaScriptUtils;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.test.util.NightModeTestUtils;
import org.chromium.ui.test.util.RenderTestRule;
import org.chromium.url.GURL;

import java.time.LocalDateTime;
import java.time.Month;
import java.time.ZoneId;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.TimeoutException;
import java.util.concurrent.atomic.AtomicReference;

/** Render tests for the {@code chrome://history/} WebUI surface on Android Desktop. */
@RunWith(ParameterizedRunner.class)
@UseRunnerDelegate(ChromeJUnit4RunnerDelegate.class)
@DoNotBatch(reason = "This test relies on native initialization and theme recreation.")
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE, "hide-scrollbars"})
@EnableFeatures(ChromeFeatureList.ANDROID_DESKTOP_WEB_UI_HISTORY)
@Restriction(DeviceFormFactor.DESKTOP)
public class HistoryWebUiRenderTest {
    private static final int TEST_TIMEOUT_SECONDS = 10;
    private static final long TEST_TIMEOUT_MS = TimeUnit.SECONDS.toMillis(TEST_TIMEOUT_SECONDS);

    // Fixed date far enough in the past to avoid relative recency labels ("Today",
    // "Yesterday", "two days ago") and keep rendered date headers deterministic.
    private static final LocalDateTime FIXED_PAST_DATE_ANCHOR =
            LocalDateTime.of(
                    /* year= */ 2025,
                    Month.JANUARY,
                    /* dayOfMonth= */ 1,
                    /* hour= */ 10,
                    /* minute= */ 30);

    @Rule
    public final FreshCtaTransitTestRule mActivityTestRule =
            ChromeTransitTestRules.freshChromeTabbedActivityRule();

    @Rule
    public final ChromeRenderTestRule mRenderTestRule =
            ChromeRenderTestRule.Builder.withPublicCorpus()
                    .setBugComponent(RenderTestRule.Component.UI_BROWSER_HISTORY)
                    .setRevision(1)
                    .setDescription("Initial WebUI history render tests on Android desktop")
                    .build();

    @Rule public final TemporaryFolder mTemporaryFolder = new TemporaryFolder();

    private ViewportTestUtils mViewportTestUtils;

    @BeforeClass
    public static void setUpBeforeActivityLaunched() {
        ChromeNightModeTestUtils.setUpNightModeBeforeChromeActivityLaunched();
    }

    @UseMethodParameterBefore(NightModeTestUtils.NightModeParams.class)
    public void setUpNightMode(boolean nightModeEnabled) {
        ChromeNightModeTestUtils.setUpNightModeForChromeActivity(nightModeEnabled);
        mRenderTestRule.setNightModeEnabled(nightModeEnabled);
    }

    @Before
    public void setUp() {
        mViewportTestUtils = new ViewportTestUtils(mActivityTestRule);
    }

    @AfterClass
    public static void tearDownAfterActivityDestroyed() {
        ChromeNightModeTestUtils.tearDownNightModeAfterChromeActivityDestroyed();
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    @UseMethodParameter(NightModeTestUtils.NightModeParams.class)
    public void testRenderEmptyHistory(boolean nightModeEnabled) throws Throwable {
        mActivityTestRule.startOnBlankPage();
        clearHistory();

        WebContents webContents = openHistoryWebUiAndWaitForLoad();
        Bitmap bitmap = takeWebContentsScreenshot(webContents);
        mRenderTestRule.compareForResult(bitmap, "history_webui_empty");
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    @UseMethodParameter(NightModeTestUtils.NightModeParams.class)
    public void testRenderPopulatedHistory(boolean nightModeEnabled) throws Throwable {
        mActivityTestRule.startOnBlankPage();
        clearHistory();

        long visitOneTimeMs = toEpochMillis(FIXED_PAST_DATE_ANCHOR);
        long visitTwoTimeMs = toEpochMillis(FIXED_PAST_DATE_ANCHOR.minusMinutes(15));
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Profile profile = ProfileManager.getLastUsedRegularProfile();
                    BrowsingHistoryBridge.addPageToHistoryForTesting(
                            profile,
                            new GURL("https://www.example.com/"),
                            "Example Domain",
                            visitOneTimeMs);
                    BrowsingHistoryBridge.addPageToHistoryForTesting(
                            profile,
                            new GURL("https://www.chromium.org/"),
                            "The Chromium Projects",
                            visitTwoTimeMs);
                });

        WebContents webContents = openHistoryWebUiAndWaitForLoad();
        Bitmap bitmap = takeWebContentsScreenshot(webContents);
        mRenderTestRule.compareForResult(bitmap, "history_webui_populated");
    }

    private static long toEpochMillis(LocalDateTime dateTime) {
        return dateTime.atZone(ZoneId.systemDefault()).toInstant().toEpochMilli();
    }

    private WebContents openHistoryWebUiAndWaitForLoad() throws Throwable {
        mActivityTestRule.loadUrl(UrlConstants.HISTORY_URL);

        WebContents webContents =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> {
                            Tab tab = mActivityTestRule.getActivityTab();
                            assertNotNull(tab);
                            assertNull(
                                    "chrome://history/ should render as WebUI, not a NativePage",
                                    tab.getNativePage());
                            WebContents contents = tab.getWebContents();
                            assertNotNull(contents);
                            return contents;
                        });

        CriteriaHelper.pollInstrumentationThread(
                () -> {
                    try {
                        String loaded =
                                JavaScriptUtils.executeJavaScriptAndWaitForResult(
                                        webContents,
                                        "!document.body.classList.contains('loading')");
                        Criteria.checkThat(loaded, Matchers.is("true"));
                    } catch (TimeoutException e) {
                        throw new CriteriaNotSatisfiedException(e);
                    }
                },
                TEST_TIMEOUT_MS,
                CriteriaHelper.DEFAULT_POLLING_INTERVAL);

        ThreadUtils.runOnUiThreadBlocking(() -> webContents.setFocus(/* hasFocus= */ false));
        mViewportTestUtils.waitForFramePresented();
        return webContents;
    }

    private void clearHistory() throws TimeoutException {
        CallbackHelper callbackHelper = new CallbackHelper();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    BrowsingDataBridge.getForProfile(ProfileManager.getLastUsedRegularProfile())
                            .clearBrowsingData(
                                    callbackHelper::notifyCalled,
                                    new int[] {BrowsingDataType.HISTORY},
                                    TimePeriod.ALL_TIME);
                });
        callbackHelper.waitForOnly(TEST_TIMEOUT_SECONDS, TimeUnit.SECONDS);
    }

    private Bitmap takeWebContentsScreenshot(WebContents webContents) throws TimeoutException {
        CallbackHelper callbackHelper = new CallbackHelper();
        AtomicReference<String> screenshotOutputPath = new AtomicReference<>();
        String outputDirPath = mTemporaryFolder.getRoot().getAbsolutePath();

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    RenderWidgetHostView rwhv = webContents.getRenderWidgetHostView();
                    assertNotNull(rwhv);
                    rwhv.writeContentBitmapToDiskAsync(
                            /* width= */ 0,
                            /* height= */ 0,
                            outputDirPath,
                            path -> {
                                screenshotOutputPath.set(path);
                                callbackHelper.notifyCalled();
                            });
                });

        callbackHelper.waitForOnly(TEST_TIMEOUT_SECONDS, TimeUnit.SECONDS);

        String path = screenshotOutputPath.get();
        assertFalse("Failed to capture WebContents screenshot", TextUtils.isEmpty(path));

        Bitmap screenshot = BitmapFactory.decodeFile(path);
        assertNotNull("Failed to decode captured screenshot", screenshot);
        return screenshot;
    }
}
