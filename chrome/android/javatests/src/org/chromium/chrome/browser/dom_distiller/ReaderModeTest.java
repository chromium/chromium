// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.dom_distiller;

import static com.google.common.truth.Truth.assertThat;

import static org.hamcrest.Matchers.is;
import static org.hamcrest.Matchers.notNullValue;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.anyFloat;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.Mockito.atLeastOnce;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import static org.chromium.base.test.transit.Triggers.noopTo;

import android.app.Activity;
import android.app.PendingIntent;

import androidx.annotation.NonNull;
import androidx.test.core.app.ApplicationProvider;
import androidx.test.filters.MediumTest;
import androidx.test.platform.app.InstrumentationRegistry;

import org.hamcrest.Matchers;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.ApplicationStatus;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.app.ChromeActivity;
import org.chromium.chrome.browser.app.tabwindow.TabWindowManagerSingleton;
import org.chromium.chrome.browser.customtabs.CustomTabActivity;
import org.chromium.chrome.browser.dom_distiller.ReaderModeManager.EntryPoint;
import org.chromium.chrome.browser.download.DownloadTestHelper;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.incognito.IncognitoNotificationServiceImpl;
import org.chromium.chrome.browser.offlinepages.OfflinePageUtils;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.R;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.chrome.test.transit.FreshCtaTransitTestRule;
import org.chromium.chrome.test.transit.dom_distiller.ReaderModeConditions.TabBackgroundColorCondition;
import org.chromium.chrome.test.transit.dom_distiller.ReaderModeConditions.TabFontSizeCondition;
import org.chromium.chrome.test.transit.dom_distiller.ReaderModePreferencesDialog;
import org.chromium.chrome.test.transit.page.CtaPageStation;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.chrome.test.util.MenuUtils;
import org.chromium.components.dom_distiller.core.DistilledPagePrefs;
import org.chromium.components.dom_distiller.core.DomDistillerFeatures;
import org.chromium.components.dom_distiller.core.DomDistillerService;
import org.chromium.components.messages.MessageDispatcher;
import org.chromium.components.messages.MessageDispatcherProvider;
import org.chromium.components.messages.MessageIdentifier;
import org.chromium.components.messages.MessageStateHandler;
import org.chromium.components.messages.MessagesTestHelper;
import org.chromium.content_public.browser.HostZoomMap;
import org.chromium.content_public.browser.test.util.TestCallbackHelperContainer;
import org.chromium.net.NetworkChangeNotifier;
import org.chromium.net.test.EmbeddedTestServer;
import org.chromium.ui.base.DeviceFormFactor;

import java.util.List;
import java.util.Objects;
import java.util.concurrent.TimeoutException;
import java.util.concurrent.atomic.AtomicBoolean;
import java.util.concurrent.atomic.AtomicReference;

/** End-to-end tests for Reader Mode (Simplified view). */
@RunWith(ChromeJUnit4ClassRunner.class)
@Restriction(DeviceFormFactor.PHONE)
@CommandLineFlags.Add({
    ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
    "--reader-mode-heuristics=alwaystrue"
})
@DisableFeatures(ChromeFeatureList.BROWSER_CONTROLS_IN_VIZ)
public class ReaderModeTest {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule
    public final FreshCtaTransitTestRule mActivityTestRule =
            ChromeTransitTestRules.freshChromeTabbedActivityRule();

    private static final String TEST_PAGE = "/chrome/test/data/dom_distiller/simple_article.html";
    // Suffix added to page titles, string is defined as IDS_DOM_DISTILLER_VIEWER_TITLE_SUFFIX in
    // dom_distiller_strings.grdp.
    private static final String TITLE_SUFFIX = " - Reading Mode";
    private static final String PAGE_TITLE = "Test Page Title" + TITLE_SUFFIX;
    private static final String CONTENT = "Lorem ipsum";

    private EmbeddedTestServer mTestServer;

    private String mURL;

    @Mock DistilledPagePrefs.Observer mTestObserver;
    private CtaPageStation mPage;

    @Before
    public void setUp() {
        mTestServer =
                EmbeddedTestServer.createAndStartServer(
                        ApplicationProvider.getApplicationContext());
        mURL = mTestServer.getURL(TEST_PAGE);
    }

    @Test
    @MediumTest
    @DisableFeatures(DomDistillerFeatures.READER_MODE_DISTILL_IN_APP)
    public void testReaderModePromptShownForIncognitoTabs() {
        mPage = mActivityTestRule.startOnBlankPage();
        // Note: For BrApp messages are only used on incognito tabs. Regular tabs use the MTB.
        mPage = mPage.openNewIncognitoTabOrWindowFast().loadWebPageProgrammatically(mURL);
        waitForReaderModeMessage();
    }

    @Test
    @MediumTest
    @DisableFeatures(DomDistillerFeatures.READER_MODE_DISTILL_IN_APP)
    public void testReaderModeInCct() throws TimeoutException {
        mPage = mActivityTestRule.startOnUrl(mURL);
        Tab originalTab = mPage.getTab();
        String innerHtml = getInnerHtml(originalTab);
        assertThat(innerHtml).doesNotContain("article-header");

        HistogramWatcher watcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "DomDistiller.Android.EntryPoint.CCT", EntryPoint.TOOLBAR_BUTTON);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    originalTab
                            .getUserDataHost()
                            .getUserData(ReaderModeManager.USER_DATA_KEY)
                            .activateReaderMode(EntryPoint.TOOLBAR_BUTTON);
                });

        CustomTabActivity customTabActivity = waitForCustomTabActivity();
        watcher.assertExpected();
        CriteriaHelper.pollUiThread(
                () -> Criteria.checkThat(customTabActivity.getActivityTab(), notNullValue()));
        @NonNull
        Tab distillerViewerTab = Objects.requireNonNull(customTabActivity.getActivityTab());
        waitForDistillation(PAGE_TITLE, distillerViewerTab);
    }

    @Test
    @MediumTest
    @EnableFeatures(DomDistillerFeatures.READER_MODE_DISTILL_IN_APP)
    public void testReaderModeInRegularTab() throws TimeoutException {
        mPage = mActivityTestRule.startOnUrl(mURL).openRegularTabAppMenu().enterReaderMode();

        Tab originalTab = mPage.getTab();
        waitForDistillation(PAGE_TITLE, originalTab);
    }

    @Test
    @MediumTest
    @DisableFeatures(DomDistillerFeatures.READER_MODE_DISTILL_IN_APP)
    public void testReaderModeInCct_Downloaded() throws TimeoutException {
        mPage = mActivityTestRule.startOnUrl(mURL);
        Tab originalTab = mPage.getTab();
        String innerHtml = getInnerHtml(originalTab);
        assertThat(innerHtml).doesNotContain("article-header");

        try (DownloadTestHelper helper =
                DownloadTestHelper.create(mActivityTestRule::getActivity)) {
            downloadAndOpenOfflinePage(helper);

            ThreadUtils.runOnUiThreadBlocking(
                    () -> {
                        originalTab
                                .getUserDataHost()
                                .getUserData(ReaderModeManager.USER_DATA_KEY)
                                .activateReaderMode(EntryPoint.APP_MENU);
                    });
            CustomTabActivity customTabActivity = waitForCustomTabActivity();
            CriteriaHelper.pollUiThread(
                    () -> Criteria.checkThat(customTabActivity.getActivityTab(), notNullValue()));

            Tab distillerViewerTab = Objects.requireNonNull(customTabActivity.getActivityTab());
            waitForDistillation(PAGE_TITLE, distillerViewerTab);
        }
    }

    @Test
    @MediumTest
    @DisableFeatures({DomDistillerFeatures.READER_MODE_DISTILL_IN_APP})
    public void testReaderModeInCct_Incognito() throws TimeoutException {
        mPage = mActivityTestRule.startOnUrl(mURL);
        openReaderModeInIncognitoCct();
    }

    @Test
    @MediumTest
    @DisabledTest(message = "https://crbug.com/1338273")
    @DisableFeatures(DomDistillerFeatures.READER_MODE_DISTILL_IN_APP)
    public void testCloseAllIncognitoNotification_ClosesCct()
            throws PendingIntent.CanceledException, TimeoutException {
        mPage = mActivityTestRule.startOnUrl(mURL);
        CustomTabActivity customTabActivity = openReaderModeInIncognitoCct();

        // Click on "Close all Incognito tabs" notification.
        PendingIntent clearIntent =
                IncognitoNotificationServiceImpl.getRemoveAllIncognitoTabsIntent(
                                ApplicationProvider.getApplicationContext())
                        .getPendingIntent();
        clearIntent.send();

        // Verify the Incognito CCT is closed.
        CriteriaHelper.pollUiThread(
                () -> {
                    Criteria.checkThat(
                            TabWindowManagerSingleton.getInstance().getIncognitoTabCount(),
                            Matchers.equalTo(0));
                });
    }

    private CustomTabActivity openReaderModeInIncognitoCct() throws TimeoutException {
        ChromeTabUtils.fullyLoadUrlInNewTab(
                InstrumentationRegistry.getInstrumentation(),
                (ChromeTabbedActivity) mActivityTestRule.getActivity(),
                mURL,
                true);

        Tab originalTab = mActivityTestRule.getActivityTab();
        assertTrue(originalTab.isIncognito());
        String innerHtml = getInnerHtml(originalTab);
        assertThat(innerHtml).doesNotContain("article-header");

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    originalTab
                            .getUserDataHost()
                            .getUserData(ReaderModeManager.USER_DATA_KEY)
                            .activateReaderMode(EntryPoint.APP_MENU);
                });
        CustomTabActivity customTabActivity = waitForCustomTabActivity();
        CriteriaHelper.pollUiThread(
                () -> Criteria.checkThat(customTabActivity.getActivityTab(), notNullValue()));
        @NonNull
        Tab distillerViewerTab = Objects.requireNonNull(customTabActivity.getActivityTab());
        waitForDistillation(PAGE_TITLE, distillerViewerTab);
        assertTrue(distillerViewerTab.isIncognito());

        return customTabActivity;
    }

    private void downloadAndOpenOfflinePage(DownloadTestHelper downloadTestHelper) {
        int callCount = downloadTestHelper.getChromeDownloadCallCount();
        MenuUtils.invokeCustomMenuActionSync(
                InstrumentationRegistry.getInstrumentation(),
                mActivityTestRule.getActivity(),
                R.id.offline_page_id);
        Assert.assertTrue(downloadTestHelper.waitForChromeDownloadToFinish(callCount));

        // Stop the server and also disconnect the network.
        mTestServer.stopAndDestroyServer();
        mTestServer = null;
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    NetworkChangeNotifier.forceConnectivityState(false);
                });

        // Load the page that has an offline copy. The offline page should be shown.
        Tab tab = mActivityTestRule.getActivityTab();
        Assert.assertFalse(isOfflinePage(tab));
        mActivityTestRule.loadUrl(ChromeTabUtils.getUrlOnUiThread(tab).getSpec());
        Assert.assertTrue(isOfflinePage(tab));
    }

    private static boolean isOfflinePage(final Tab tab) {
        AtomicBoolean isOffline = new AtomicBoolean();
        ThreadUtils.runOnUiThreadBlocking(() -> isOffline.set(OfflinePageUtils.isOfflinePage(tab)));
        return isOffline.get();
    }

    @Test
    @MediumTest
    @DisableFeatures(DomDistillerFeatures.READER_MODE_DISTILL_IN_APP)
    public void testPreferenceInCct() throws TimeoutException {
        mPage = mActivityTestRule.startOnUrl(mURL);
        Tab originalTab = mPage.getTab();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    originalTab
                            .getUserDataHost()
                            .getUserData(ReaderModeManager.USER_DATA_KEY)
                            .activateReaderMode(EntryPoint.APP_MENU);
                });
        CustomTabActivity customTabActivity = waitForCustomTabActivity();
        CriteriaHelper.pollUiThread(() -> customTabActivity.getActivityTab() != null);
        @NonNull
        Tab distillerViewerTab = Objects.requireNonNull(customTabActivity.getActivityTab());
        waitForDistillation(PAGE_TITLE, distillerViewerTab);

        doTestSettingPreferences(customTabActivity, distillerViewerTab);
    }

    @Test
    @MediumTest
    @EnableFeatures(DomDistillerFeatures.READER_MODE_DISTILL_IN_APP)
    public void testPreferenceInTab() throws TimeoutException {
        mPage = mActivityTestRule.startOnUrl(mURL).openRegularTabAppMenu().enterReaderMode();

        Tab tab = mPage.getTab();
        waitForDistillation(PAGE_TITLE, tab);

        doTestSettingPreferences(mActivityTestRule.getActivity(), tab);
    }

    @Test
    @MediumTest
    public void testZoomLevelPrefsCallbackUpdatesFontScaling() throws TimeoutException {
        mPage = mActivityTestRule.startOnBlankPage();
        final DistilledPagePrefs distilledPagePrefs = getDistilledPagePrefs();

        // Check that the initial font scaling is tied to the default zoom level.
        final double initialZoomLevel =
                ThreadUtils.runOnUiThreadBlocking(
                        () ->
                                HostZoomMap.getDefaultZoomLevel(
                                        mActivityTestRule.getActivityTab().getProfile()));
        final float initialZoomFactor = (float) Math.pow(1.2, initialZoomLevel);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Assert.assertEquals(
                            initialZoomFactor, distilledPagePrefs.getFontScaling(), 0.001f);
                });

        // Change the default zoom level and ensure the distilled page prefs are
        // updated to reflect the change.
        final double newZoomLevel = 2.0;
        final float newZoomFactor = (float) Math.pow(1.2, newZoomLevel);

        final CallbackHelper fontScalingChangedCallback = new CallbackHelper();
        DistilledPagePrefs.Observer observer =
                new DistilledPagePrefs.Observer() {
                    @Override
                    public void onChangeTheme(int theme) {}

                    @Override
                    public void onChangeFontFamily(int font) {}

                    @Override
                    public void onChangeFontScaling(float fontScaling) {
                        if (Math.abs(fontScaling - newZoomFactor) < 0.001f) {
                            fontScalingChangedCallback.notifyCalled();
                        }
                    }
                };
        ThreadUtils.runOnUiThreadBlocking(() -> distilledPagePrefs.addObserver(observer));

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    HostZoomMap.setDefaultZoomLevel(
                            mActivityTestRule.getActivityTab().getProfile(), newZoomLevel);
                });

        fontScalingChangedCallback.waitForCallback(0);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Assert.assertEquals(newZoomFactor, distilledPagePrefs.getFontScaling(), 0.001f);
                    distilledPagePrefs.removeObserver(observer);
                });
    }

    /**
     * Wait until a {@link CustomTabActivity} shows up, and return it.
     *
     * @return a {@link CustomTabActivity}
     */
    private CustomTabActivity waitForCustomTabActivity() {
        AtomicReference<CustomTabActivity> activity = new AtomicReference<>();
        CriteriaHelper.pollUiThread(
                () -> {
                    for (Activity runningActivity : ApplicationStatus.getRunningActivities()) {
                        if (runningActivity instanceof CustomTabActivity) {
                            activity.set((CustomTabActivity) runningActivity);
                            return true;
                        }
                    }
                    return false;
                });
        return activity.get();
    }

    private DistilledPagePrefs getDistilledPagePrefs() {
        AtomicReference<DistilledPagePrefs> prefs = new AtomicReference<>();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    DomDistillerService domDistillerService =
                            DomDistillerServiceFactory.getForProfile(
                                    ProfileManager.getLastUsedRegularProfile());
                    prefs.set(domDistillerService.getDistilledPagePrefs());
                });
        return prefs.get();
    }

    private void doTestSettingPreferences(ChromeActivity activity, Tab tab) {
        DistilledPagePrefs prefs = getDistilledPagePrefs();
        ThreadUtils.runOnUiThreadBlocking(() -> prefs.addObserver(mTestObserver));

        noopTo().waitFor(new TabBackgroundColorCondition(tab, "\"rgb(255, 255, 255)\""));

        ReaderModePreferencesDialog dialog = ReaderModePreferencesDialog.open(activity);

        // Test setting background color
        dialog.darkButtonElement
                .clickTo()
                .waitFor(new TabBackgroundColorCondition(tab, "\"rgb(32, 33, 36)\""));
        dialog.sepiaButtonElement
                .clickTo()
                .waitFor(new TabBackgroundColorCondition(tab, "\"rgb(254, 247, 224)\""));
        dialog.lightButtonElement
                .clickTo()
                .waitFor(new TabBackgroundColorCondition(tab, "\"rgb(255, 255, 255)\""));
        verify(mTestObserver, times(3)).onChangeTheme(anyInt());

        // Test setting font size
        String fontSizeDefault = "\"16px\"";
        String fontSizeMax = "\"32px\"";
        String fontSizeMin = "\"8px\"";

        // CCT and in-app apply different CSS.
        if (activity instanceof CustomTabActivity) {
            fontSizeDefault = "\"14px\"";
            fontSizeMax = "\"28px\"";
            fontSizeMin = "\"7px\"";
        }

        noopTo().waitFor(new TabFontSizeCondition(tab, fontSizeDefault));
        // Max is 200% font size.
        dialog.setFontSizeSliderToMaxTo().waitFor(new TabFontSizeCondition(tab, fontSizeMax));
        verify(mTestObserver, atLeastOnce()).onChangeFontScaling(anyFloat());
        // Min is 50% font size.
        dialog.setFontSizeSliderToMinTo().waitFor(new TabFontSizeCondition(tab, fontSizeMin));
        verify(mTestObserver, atLeastOnce()).onChangeFontScaling(anyFloat());

        // TODO(crbug.com/40125950): change font family as well.

        dialog.pressBackTo().dropCarryOn();
    }

    /**
     * Run JavaScript on a certain {@link Tab}.
     *
     * @param tab The tab to be injected to.
     * @param javaScript The JavaScript code to be injected.
     * @return The result of the code.
     */
    private String runJavaScript(Tab tab, String javaScript) throws TimeoutException {
        TestCallbackHelperContainer.OnEvaluateJavaScriptResultHelper javascriptHelper =
                new TestCallbackHelperContainer.OnEvaluateJavaScriptResultHelper();
        javascriptHelper.evaluateJavaScriptForTests(tab.getWebContents(), javaScript);
        javascriptHelper.waitUntilHasValue();
        return javascriptHelper.getJsonResultAndClear();
    }

    /**
     * @param tab The tab to be inspected.
     * @return The inner HTML of a certain {@link Tab}.
     */
    private String getInnerHtml(Tab tab) throws TimeoutException {
        return runJavaScript(tab, "document.body.innerHTML");
    }

    /** Wait until a Reader Mode message shows up. */
    private void waitForReaderModeMessage() {
        CriteriaHelper.pollUiThread(
                () -> {
                    MessageDispatcher messageDispatcher =
                            ThreadUtils.runOnUiThreadBlocking(
                                    () ->
                                            MessageDispatcherProvider.from(
                                                    mActivityTestRule
                                                            .getActivity()
                                                            .getWindowAndroid()));
                    List<MessageStateHandler> messages =
                            MessagesTestHelper.getEnqueuedMessages(
                                    messageDispatcher, MessageIdentifier.READER_MODE);
                    return messages.size() > 0;
                });
    }

    /**
     * Wait until the distilled content is shown on the {@link Tab}.
     *
     * @param expectedTitle The expected title of the distilled content
     * @param tab the tab to wait
     */
    private void waitForDistillation(
            @SuppressWarnings("SameParameterValue") String expectedTitle, Tab tab)
            throws TimeoutException {
        CriteriaHelper.pollUiThread(
                () ->
                        Criteria.checkThat(
                                ChromeTabUtils.getUrlOnUiThread(tab).getScheme(),
                                is("chrome-distiller")));
        ChromeTabUtils.waitForTabPageLoaded(tab, null);
        // Distiller Viewer load the content dynamically, so waitForTabPageLoaded() is not enough.
        CriteriaHelper.pollUiThreadLongTimeout(
                null, () -> Criteria.checkThat(tab.getTitle(), is(expectedTitle)));

        String innerHtml = getInnerHtml(tab);
        assertThat(innerHtml).contains("article-header");
        assertThat(innerHtml).contains(CONTENT);
    }
}
