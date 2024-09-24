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
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import static org.chromium.chrome.browser.dom_distiller.ReaderModeManager.DOM_DISTILLER_SCHEME;

import android.app.Activity;
import android.app.PendingIntent;

import androidx.annotation.NonNull;
import androidx.test.core.app.ApplicationProvider;
import androidx.test.filters.MediumTest;
import androidx.test.platform.app.InstrumentationRegistry;

import org.hamcrest.Matchers;
import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import org.chromium.base.ApplicationStatus;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.transit.Condition;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.app.ChromeActivity;
import org.chromium.chrome.browser.customtabs.CustomTabActivity;
import org.chromium.chrome.browser.download.DownloadTestRule;
import org.chromium.chrome.browser.download.DownloadTestRule.CustomMainActivityStart;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.incognito.IncognitoNotificationServiceImpl;
import org.chromium.chrome.browser.offlinepages.OfflinePageUtils;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.R;
import org.chromium.chrome.test.transit.dom_distiller.ReaderModeConditions.TabBackgroundColorCondition;
import org.chromium.chrome.test.transit.dom_distiller.ReaderModeConditions.TabFontSizeCondition;
import org.chromium.chrome.test.transit.dom_distiller.ReaderModePreferencesDialog;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.chrome.test.util.MenuUtils;
import org.chromium.components.dom_distiller.core.DistilledPagePrefs;
import org.chromium.components.dom_distiller.core.DomDistillerService;
import org.chromium.components.dom_distiller.core.DomDistillerUrlUtils;
import org.chromium.components.messages.MessageDispatcher;
import org.chromium.components.messages.MessageDispatcherProvider;
import org.chromium.components.messages.MessageIdentifier;
import org.chromium.components.messages.MessageStateHandler;
import org.chromium.components.messages.MessagesTestHelper;
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
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@DisableFeatures(ChromeFeatureList.BROWSER_CONTROLS_IN_VIZ)
public class ReaderModeTest implements CustomMainActivityStart {
    @Rule public DownloadTestRule mDownloadTestRule = new DownloadTestRule(this);

    private static final String TEST_PAGE = "/chrome/test/data/dom_distiller/simple_article.html";
    // Suffix added to page titles, string is defined as IDS_DOM_DISTILLER_VIEWER_TITLE_SUFFIX in
    // dom_distiller_strings.grdp.
    private static final String TITLE_SUFFIX = " - Simplified View";
    private static final String PAGE_TITLE = "Test Page Title" + TITLE_SUFFIX;
    private static final String CONTENT = "Lorem ipsum";

    @SuppressWarnings("FieldCanBeLocal")
    private EmbeddedTestServer mTestServer;

    private String mURL;

    @Mock DistilledPagePrefs.Observer mTestObserver;

    @Override
    public void customMainActivityStart() {
        MockitoAnnotations.initMocks(this);
        mTestServer =
                EmbeddedTestServer.createAndStartServer(
                        ApplicationProvider.getApplicationContext());
        mURL = mTestServer.getURL(TEST_PAGE);
        mDownloadTestRule.startMainActivityWithURL(mURL);
    }

    @Test
    @MediumTest
    @DisabledTest(message = "crbug.com/1402815")
    public void testReaderModePromptShown() {
        waitForReaderModeMessage();
    }

    @Test
    @MediumTest
    @EnableFeatures(ChromeFeatureList.READER_MODE_IN_CCT)
    public void testReaderModeInCCT() throws TimeoutException {
        Tab originalTab = mDownloadTestRule.getActivity().getActivityTab();
        String innerHtml = getInnerHtml(originalTab);
        assertThat(innerHtml).doesNotContain("article-header");

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    originalTab
                            .getUserDataHost()
                            .getUserData(ReaderModeManager.USER_DATA_KEY)
                            .activateReaderMode();
                });
        CustomTabActivity customTabActivity = waitForCustomTabActivity();
        CriteriaHelper.pollUiThread(
                () -> Criteria.checkThat(customTabActivity.getActivityTab(), notNullValue()));
        @NonNull
        Tab distillerViewerTab = Objects.requireNonNull(customTabActivity.getActivityTab());
        waitForDistillation(PAGE_TITLE, distillerViewerTab);
    }

    @Test
    @MediumTest
    @EnableFeatures(ChromeFeatureList.READER_MODE_IN_CCT)
    public void testReaderModeInCCT_Downloaded() throws TimeoutException {
        Tab originalTab = mDownloadTestRule.getActivity().getActivityTab();
        String innerHtml = getInnerHtml(originalTab);
        assertThat(innerHtml).doesNotContain("article-header");

        downloadAndOpenOfflinePage();

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    originalTab
                            .getUserDataHost()
                            .getUserData(ReaderModeManager.USER_DATA_KEY)
                            .activateReaderMode();
                });
        CustomTabActivity customTabActivity = waitForCustomTabActivity();
        CriteriaHelper.pollUiThread(
                () -> Criteria.checkThat(customTabActivity.getActivityTab(), notNullValue()));
        @NonNull
        Tab distillerViewerTab = Objects.requireNonNull(customTabActivity.getActivityTab());
        waitForDistillation(PAGE_TITLE, distillerViewerTab);
    }

    @Test
    @MediumTest
    @EnableFeatures(ChromeFeatureList.READER_MODE_IN_CCT)
    public void testReaderModeInCCT_Incognito() throws TimeoutException {
        openReaderModeInIncognitoCCT();
    }

    @Test
    @MediumTest
    @EnableFeatures(ChromeFeatureList.READER_MODE_IN_CCT)
    @DisabledTest(message = "https://crbug.com/1338273")
    public void testCloseAllIncognitoNotification_ClosesCCT()
            throws PendingIntent.CanceledException, TimeoutException {
        CustomTabActivity customTabActivity = openReaderModeInIncognitoCCT();

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
                            customTabActivity.getTabModelSelector().getModel(true).getCount(),
                            Matchers.equalTo(0));
                });
    }

    private CustomTabActivity openReaderModeInIncognitoCCT() throws TimeoutException {
        ChromeTabUtils.fullyLoadUrlInNewTab(
                InstrumentationRegistry.getInstrumentation(),
                (ChromeTabbedActivity) mDownloadTestRule.getActivity(),
                mURL,
                true);

        Tab originalTab = mDownloadTestRule.getActivity().getActivityTab();
        assertTrue(originalTab.isIncognito());
        String innerHtml = getInnerHtml(originalTab);
        assertThat(innerHtml).doesNotContain("article-header");

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    originalTab
                            .getUserDataHost()
                            .getUserData(ReaderModeManager.USER_DATA_KEY)
                            .activateReaderMode();
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

    private void downloadAndOpenOfflinePage() {
        int callCount = mDownloadTestRule.getChromeDownloadCallCount();
        MenuUtils.invokeCustomMenuActionSync(
                InstrumentationRegistry.getInstrumentation(),
                mDownloadTestRule.getActivity(),
                R.id.offline_page_id);
        Assert.assertTrue(mDownloadTestRule.waitForChromeDownloadToFinish(callCount));

        // Stop the server and also disconnect the network.
        mTestServer.stopAndDestroyServer();
        mTestServer = null;
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    NetworkChangeNotifier.forceConnectivityState(false);
                });

        // Load the page that has an offline copy. The offline page should be shown.
        Tab tab = mDownloadTestRule.getActivity().getActivityTab();
        Assert.assertFalse(isOfflinePage(tab));
        mDownloadTestRule.loadUrl(ChromeTabUtils.getUrlOnUiThread(tab).getSpec());
        Assert.assertTrue(isOfflinePage(tab));
    }

    private static boolean isOfflinePage(final Tab tab) {
        AtomicBoolean isOffline = new AtomicBoolean();
        ThreadUtils.runOnUiThreadBlocking(() -> isOffline.set(OfflinePageUtils.isOfflinePage(tab)));
        return isOffline.get();
    }

    @Test
    @MediumTest
    @DisableFeatures(ChromeFeatureList.READER_MODE_IN_CCT)
    public void testReaderModeInTab() throws TimeoutException {
        Tab tab = mDownloadTestRule.getActivity().getActivityTab();
        String innerHtml = getInnerHtml(tab);
        assertThat(innerHtml).doesNotContain("article-header");

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    tab.getUserDataHost()
                            .getUserData(ReaderModeManager.USER_DATA_KEY)
                            .activateReaderMode();
                });
        waitForDistillation(PAGE_TITLE, mDownloadTestRule.getActivity().getActivityTab());
    }

    @Test
    @MediumTest
    @EnableFeatures(ChromeFeatureList.READER_MODE_IN_CCT)
    public void testPreferenceInCCT() throws TimeoutException {
        Tab originalTab = mDownloadTestRule.getActivity().getActivityTab();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    originalTab
                            .getUserDataHost()
                            .getUserData(ReaderModeManager.USER_DATA_KEY)
                            .activateReaderMode();
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
    @DisableFeatures(ChromeFeatureList.READER_MODE_IN_CCT)
    public void testPreferenceInTab() throws TimeoutException {
        mDownloadTestRule.loadUrl(
                DomDistillerUrlUtils.getDistillerViewUrlFromUrl(
                        DOM_DISTILLER_SCHEME, mURL, PAGE_TITLE));

        Tab tab = mDownloadTestRule.getActivity().getActivityTab();
        waitForDistillation(PAGE_TITLE, tab);

        doTestSettingPreferences(mDownloadTestRule.getActivity(), tab);
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
        prefs.addObserver(mTestObserver);

        Condition.runAndWaitFor(
                /* trigger= */ null,
                new TabBackgroundColorCondition(tab, "\"rgb(255, 255, 255)\""));

        ReaderModePreferencesDialog dialog = ReaderModePreferencesDialog.open(activity);

        // Test setting background color
        dialog.pickColorDark(new TabBackgroundColorCondition(tab, "\"rgb(32, 33, 36)\""));
        dialog.pickColorSepia(new TabBackgroundColorCondition(tab, "\"rgb(254, 247, 224)\""));
        dialog.pickColorLight(new TabBackgroundColorCondition(tab, "\"rgb(255, 255, 255)\""));
        verify(mTestObserver, times(3)).onChangeTheme(anyInt());

        // Test setting font size
        Condition.runAndWaitFor(/* trigger= */ null, new TabFontSizeCondition(tab, "\"14px\""));
        // Max is 200% font size.
        dialog.setFontSizeSliderToMax(new TabFontSizeCondition(tab, "\"28px\""));
        // Min is 50% font size.
        dialog.setFontSizeSliderToMin(new TabFontSizeCondition(tab, "\"7px\""));
        verify(mTestObserver, times(2)).onChangeFontScaling(anyFloat());

        // TODO(crbug.com/40125950): change font family as well.

        dialog.pressBackToClose();
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
                                                    mDownloadTestRule
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
        CriteriaHelper.pollUiThread(() -> Criteria.checkThat(tab.getTitle(), is(expectedTitle)));

        String innerHtml = getInnerHtml(tab);
        assertThat(innerHtml).contains("article-header");
        assertThat(innerHtml).contains(CONTENT);
    }
}
