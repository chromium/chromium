// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar;

import static org.junit.Assert.assertEquals;
import static org.mockito.Mockito.atLeast;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import android.content.Context;

import androidx.test.filters.MediumTest;
import androidx.test.filters.SmallTest;
import androidx.test.platform.app.InstrumentationRegistry;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.test.params.ParameterAnnotations;
import org.chromium.base.test.params.ParameterProvider;
import org.chromium.base.test.params.ParameterSet;
import org.chromium.base.test.params.ParameterizedRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.UrlUtils;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider.ControlsPosition;
import org.chromium.chrome.browser.dom_distiller.DomDistillerTabUtils;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.omnibox.LocationBarDataProvider;
import org.chromium.chrome.browser.omnibox.NewTabPageDelegate;
import org.chromium.chrome.browser.omnibox.UrlBarData;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.browser.tab.MockTab;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabObserver;
import org.chromium.chrome.browser.tab.TabSelectionType;
import org.chromium.chrome.browser.toolbar.top.ToolbarLayout;
import org.chromium.chrome.browser.url_constants.UrlConstantResolver;
import org.chromium.chrome.test.ChromeJUnit4RunnerDelegate;
import org.chromium.chrome.test.transit.AutoResetCtaTransitTestRule;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.chrome.test.transit.page.WebPageStation;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.content_public.browser.NavigationHandle;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.test.util.JavaScriptUtils;
import org.chromium.url.GURL;
import org.chromium.url.JUnitTestGURLs;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import java.util.concurrent.atomic.AtomicBoolean;
import java.util.concurrent.atomic.AtomicReference;

/** Tests for LocationBarModel. */
@RunWith(ParameterizedRunner.class)
@ParameterAnnotations.UseRunnerDelegate(ChromeJUnit4RunnerDelegate.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Batch(Batch.PER_CLASS)
public class LocationBarModelTest {
    @Rule
    public AutoResetCtaTransitTestRule mActivityTestRule =
            ChromeTransitTestRules.autoResetCtaActivityRule();

    private WebPageStation mPage;
    private ToolbarDataProvider.Observer mToolbarObserver;
    private LocationBarDataProvider.Observer mLocationBarObserver;

    @Before
    public void setUp() throws InterruptedException {
        mPage = mActivityTestRule.startOnBlankPage();
    }

    @After
    public void tearDown() {
        if (mToolbarObserver != null || mLocationBarObserver != null) {
            ThreadUtils.runOnUiThreadBlocking(
                    () -> {
                        LocationBarModel model =
                                mActivityTestRule
                                        .getActivity()
                                        .getToolbarManager()
                                        .getLocationBarModelForTesting();
                        if (mToolbarObserver != null) {
                            model.removeToolbarDataProviderObserver(mToolbarObserver);
                            mToolbarObserver = null;
                        }
                        if (mLocationBarObserver != null) {
                            model.removeObserver(mLocationBarObserver);
                            mLocationBarObserver = null;
                        }
                    });
        }
    }

    /**
     * After closing all {@link Tab}s, the {@link LocationBarModel} should know that it is not
     * showing any {@link Tab}.
     */
    @Test
    @Feature({"Android-Toolbar"})
    @MediumTest
    public void testClosingLastTabReflectedInModel() {
        Assert.assertNotSame(
                "No current tab",
                Tab.INVALID_TAB_ID,
                getCurrentTabId(mActivityTestRule.getActivity()));
        ChromeTabUtils.closeCurrentTab(
                InstrumentationRegistry.getInstrumentation(), mActivityTestRule.getActivity());
        assertEquals(
                "Didn't close all tabs.",
                0,
                ChromeTabUtils.getNumOpenTabs(mActivityTestRule.getActivity()));
        assertEquals(
                "LocationBarModel is still trying to show a tab.",
                Tab.INVALID_TAB_ID,
                getCurrentTabId(mActivityTestRule.getActivity()));
    }

    @Test
    @SmallTest
    public void testDisplayAndEditText() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    TestLocationBarModel model =
                            new TestLocationBarModel(mActivityTestRule.getActivity());
                    model.setVisibleGurl(UrlConstantResolver.getOriginalNtpGurl());
                    assertDisplayAndEditText(model, "", null);

                    model.setVisibleGurl(JUnitTestGURLs.CHROME_ABOUT);
                    model.setDisplayUrl("chrome://about");
                    model.setFullUrl("chrome://about");
                    assertDisplayAndEditText(model, "chrome://about", "chrome://about");

                    model.setVisibleGurl(JUnitTestGURLs.URL_1);
                    model.setDisplayUrl("https://one.com");
                    model.setFullUrl("https://one.com");
                    assertDisplayAndEditText(model, "https://one.com", "https://one.com");

                    model.setDisplayUrl("one.com");
                    assertDisplayAndEditText(model, "one.com", "https://one.com");

                    // https://crbug.com/40056044
                    model.setVisibleGurl(GURL.emptyGURL());
                    model.setDisplayUrl("about:blank");
                    model.setFullUrl("about:blank");
                    assertDisplayAndEditText(model, "about:blank", "about:blank");

                    model.destroy();
                });
    }

    /** Provides parameters for different types of transitions between tabs. */
    public static class IncognitoTransitionParamProvider implements ParameterProvider {
        @Override
        public Iterable<ParameterSet> getParameters() {
            List<ParameterSet> result = new ArrayList<>(8);
            for (boolean fromIncognito : Arrays.asList(true, false)) {
                for (boolean toIncognito : Arrays.asList(true, false)) {
                    result.add(
                            new ParameterSet()
                                    .value(fromIncognito, toIncognito)
                                    .name(
                                            String.format(
                                                    "from_%b_to_%b", fromIncognito, toIncognito)));
                }
            }
            return result;
        }
    }

    @Test
    @MediumTest
    @ParameterAnnotations.UseMethodParameter(IncognitoTransitionParamProvider.class)
    // TODO(crbug.com/457847264): Restrict to phones after launch.
    @DisableFeatures(ChromeFeatureList.ANDROID_OPEN_INCOGNITO_AS_WINDOW)
    public void testOnIncognitoStateChange_toolbarDataProvider(
            boolean fromIncognito, boolean toIncognito) {
        AtomicReference<Integer> incognitoStateObserverCallCount =
                new AtomicReference<>(Integer.valueOf(0));
        // Add a regular tab next to the one created in setup.
        mActivityTestRule.loadUrlInNewTab("about:blank", /* incognito= */ false);
        // Add two incognito tabs.
        mActivityTestRule.loadUrlInNewTab("about:blank", /* incognito= */ true);
        mActivityTestRule.loadUrlInNewTab("about:blank", /* incognito= */ true);

        ChromeTabbedActivity activity = mActivityTestRule.getActivity();
        LocationBarModel locationBarModel =
                activity.getToolbarManager().getLocationBarModelForTesting();
        mToolbarObserver =
                new ToolbarDataProvider.Observer() {
                    @Override
                    public void onIncognitoStateChanged() {
                        assertEquals(toIncognito, locationBarModel.isIncognito());
                        incognitoStateObserverCallCount.set(Integer.valueOf(1));
                    }
                };

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mActivityTestRule
                            .getActivity()
                            .getTabModelSelector()
                            .selectModel(fromIncognito);
                    locationBarModel.addToolbarDataProviderObserver(mToolbarObserver);

                    // Switch to an existing tab.
                    mActivityTestRule
                            .getActivity()
                            .getTabModelSelector()
                            .selectModel(/* incognito= */ toIncognito);
                    mActivityTestRule
                            .getActivity()
                            .getTabModelSelector()
                            .getCurrentModel()
                            .setIndex(0, TabSelectionType.FROM_USER);
                });

        assertEquals(toIncognito, locationBarModel.isIncognito());
        if (fromIncognito != toIncognito) {
            assertEquals(Integer.valueOf(1), incognitoStateObserverCallCount.get());
        } else {
            assertEquals(Integer.valueOf(0), incognitoStateObserverCallCount.get());
        }
    }

    @Test
    @MediumTest
    @ParameterAnnotations.UseMethodParameter(IncognitoTransitionParamProvider.class)
    // TODO(crbug.com/457847264): Restrict to phones after launch.
    @DisableFeatures(ChromeFeatureList.ANDROID_OPEN_INCOGNITO_AS_WINDOW)
    public void testOnIncognitoStateChange_switchTab(boolean fromIncognito, boolean toIncognito) {
        // Add a regular tab next to the one created in setup.
        mActivityTestRule.loadUrlInNewTab("about:blank", /* incognito= */ false);
        // Add two incognito tabs.
        mActivityTestRule.loadUrlInNewTab("about:blank", /* incognito= */ true);
        mActivityTestRule.loadUrlInNewTab("about:blank", /* incognito= */ true);

        ChromeTabbedActivity activity = mActivityTestRule.getActivity();
        LocationBarModel locationBarModel =
                activity.getToolbarManager().getLocationBarModelForTesting();
        mLocationBarObserver = mock(LocationBarDataProvider.Observer.class);
        doAnswer(
                        (invocation) -> {
                            assertEquals(toIncognito, locationBarModel.isIncognito());
                            return null;
                        })
                .when(mLocationBarObserver)
                .onIncognitoStateChanged();

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mActivityTestRule
                            .getActivity()
                            .getTabModelSelector()
                            .selectModel(fromIncognito);
                    locationBarModel.addObserver(mLocationBarObserver);

                    // Switch to an existing tab.
                    mActivityTestRule
                            .getActivity()
                            .getTabModelSelector()
                            .selectModel(/* incognito= */ toIncognito);
                    mActivityTestRule
                            .getActivity()
                            .getTabModelSelector()
                            .getCurrentModel()
                            .setIndex(0, TabSelectionType.FROM_USER);
                });

        assertEquals(toIncognito, locationBarModel.isIncognito());
        if (fromIncognito != toIncognito) {
            verify(mLocationBarObserver).onIncognitoStateChanged();
        } else {
            verify(mLocationBarObserver, times(0)).onIncognitoStateChanged();
        }
    }

    @Test
    @MediumTest
    @ParameterAnnotations.UseMethodParameter(IncognitoTransitionParamProvider.class)
    // TODO(crbug.com/457847264): Restrict to phones after launch.
    @DisableFeatures(ChromeFeatureList.ANDROID_OPEN_INCOGNITO_AS_WINDOW)
    public void testOnIncognitoStateChange_newTab(boolean fromIncognito, boolean toIncognito) {
        // Add a regular tab next to the one created in setup.
        mActivityTestRule.loadUrlInNewTab("about:blank", /* incognito= */ false);
        // Add two incognito tabs.
        mActivityTestRule.loadUrlInNewTab("about:blank", /* incognito= */ true);
        mActivityTestRule.loadUrlInNewTab("about:blank", /* incognito= */ true);

        ChromeTabbedActivity activity = mActivityTestRule.getActivity();
        LocationBarModel locationBarModel =
                activity.getToolbarManager().getLocationBarModelForTesting();
        mLocationBarObserver = mock(LocationBarDataProvider.Observer.class);
        doAnswer(
                        (invocation) -> {
                            assertEquals(toIncognito, locationBarModel.isIncognito());
                            return null;
                        })
                .when(mLocationBarObserver)
                .onIncognitoStateChanged();

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mActivityTestRule
                            .getActivity()
                            .getTabModelSelector()
                            .selectModel(fromIncognito);
                    locationBarModel.addObserver(mLocationBarObserver);
                });

        // Switch to a new tab.
        mActivityTestRule.loadUrlInNewTab("about:blank", toIncognito);

        assertEquals(toIncognito, locationBarModel.isIncognito());
        if (fromIncognito != toIncognito) {
            verify(mLocationBarObserver).onIncognitoStateChanged();
        } else {
            verify(mLocationBarObserver, times(0)).onIncognitoStateChanged();
        }
    }

    @Test
    @MediumTest
    public void testOnSecurityStateChanged() {
        LocationBarModel locationBarModel =
                mActivityTestRule.getActivity().getToolbarManager().getLocationBarModelForTesting();
        mLocationBarObserver = mock(LocationBarDataProvider.Observer.class);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    locationBarModel.addObserver(mLocationBarObserver);
                });

        mActivityTestRule.loadUrl(UrlUtils.encodeHtmlDataUri("test content"));

        verify(mLocationBarObserver, atLeast(1)).onSecurityStateChanged();
    }

    /**
     * Repro for b/514078082: a same-document traversal that is aborted via the Navigation API never
     * results in TabObserver#onDidFinishNavigationEnd() being dispatched (TabWebContentsObserver
     * early-returns on !navigation.hasCommitted()), so LocationBarModel#mIsInSameDocNav stays
     * latched and all but the first subsequent security state change is silently dropped.
     */
    @Test
    @MediumTest
    public void testAbortedSameDocTraversalLatchesSameDocNavFlag() throws Exception {
        ChromeTabbedActivity activity = mActivityTestRule.getActivity();
        LocationBarModel locationBarModel =
                activity.getToolbarManager().getLocationBarModelForTesting();

        mActivityTestRule.loadUrl(
                mActivityTestRule
                        .getTestServer()
                        .getURL("/chrome/test/data/android/navigate/simple.html"));

        Tab tab = ThreadUtils.runOnUiThreadBlocking(() -> activity.getActivityTab());
        WebContents webContents = ThreadUtils.runOnUiThreadBlocking(tab::getWebContents);
        AtomicBoolean sawAbortedSameDocNav = new AtomicBoolean();
        AtomicBoolean sawNavigationEndAfterAbort = new AtomicBoolean();
        TabObserver navObserver =
                new TabObserver() {
                    @Override
                    public void onDidFinishNavigationInPrimaryMainFrame(
                            Tab tab, NavigationHandle navigation) {
                        if (navigation.isSameDocument() && !navigation.hasCommitted()) {
                            sawAbortedSameDocNav.set(true);
                        }
                    }

                    @Override
                    public void onDidFinishNavigationEnd() {
                        // The committed pushState that sets up the traversal also dispatches
                        // this, so only the dispatch following the aborted navigation counts.
                        if (sawAbortedSameDocNav.get()) {
                            sawNavigationEndAfterAbort.set(true);
                        }
                    }
                };
        ThreadUtils.runOnUiThreadBlocking(() -> tab.addObserver(navObserver));

        // Start a same-document traversal and cancel it from the 'navigate' event.
        JavaScriptUtils.executeJavaScriptAndWaitForResult(
                webContents,
                "(function() {"
                        + "  history.pushState({}, '', '#a');"
                        + "  navigation.addEventListener('navigate', e => {"
                        + "    if (e.navigationType === 'traverse') e.preventDefault();"
                        + "  });"
                        + "  history.back();"
                        + "  return 'ok';"
                        + "})()");

        CriteriaHelper.pollUiThread(
                sawAbortedSameDocNav::get,
                "Never saw a same-document DidFinishNavigation with hasCommitted() == false");
        ThreadUtils.runOnUiThreadBlocking(() -> tab.removeObserver(navObserver));

        Assert.assertTrue(
                "onDidFinishNavigationEnd() was never dispatched for the aborted navigation,"
                        + " so LocationBarModel's same-document debounce stays latched.",
                sawNavigationEndAfterAbort.get());

        // With the flag latched, the first notification is delivered and every later one is
        // dropped, so a page can pin the omnibox security state.
        mLocationBarObserver = mock(LocationBarDataProvider.Observer.class);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    locationBarModel.addObserver(mLocationBarObserver);
                    locationBarModel.notifySecurityStateChanged();
                    locationBarModel.notifySecurityStateChanged();
                });

        verify(mLocationBarObserver, times(2)).onSecurityStateChanged();
    }

    /**
     * The same-document debounce is scoped to one navigation in one tab. If setTab() carries a
     * latched debounce into the incoming tab, updateVisibleGurl() short-circuits while
     * broadcastUrlChanged() still fires (because isTabChanging is true), so the omnibox redraws
     * using the *previous* tab's cached URL.
     */
    @Test
    @MediumTest
    public void testSetTabResetsSameDocNavFlags() {
        ChromeTabbedActivity activity = mActivityTestRule.getActivity();
        LocationBarModel locationBarModel =
                activity.getToolbarManager().getLocationBarModelForTesting();

        String urlA =
                mActivityTestRule
                        .getTestServer()
                        .getURL("/chrome/test/data/android/navigate/one.html");
        String urlB =
                mActivityTestRule
                        .getTestServer()
                        .getURL("/chrome/test/data/android/navigate/two.html");

        mActivityTestRule.loadUrl(urlA);
        // Pre-load the second tab. Switching to a frozen tab dispatches onContentChanged, which
        // resets the flags on its own and would mask the bug this test is guarding against.
        mActivityTestRule.loadUrlInNewTab(urlB, /* incognito= */ false);
        ChromeTabUtils.switchTabInCurrentTabModel(activity, 0);

        assertEquals(
                urlA,
                ThreadUtils.runOnUiThreadBlocking(
                        () -> locationBarModel.getCurrentGurl().getSpec()));

        // Latch the debounce on tab A, the state an aborted same-document traversal leaves behind.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    locationBarModel.notifyDidStartNavigation(/* isSameDocument= */ true);
                    // isTabChanging=true forces the broadcast that arms the URL flag without
                    // requiring an actual URL change.
                    locationBarModel.notifyUrlChanged(/* isTabChanging= */ true);
                    locationBarModel.notifySecurityStateChanged();
                });

        ChromeTabUtils.switchTabInCurrentTabModel(activity, 1);

        CriteriaHelper.pollUiThread(
                () -> urlB.equals(locationBarModel.getCurrentGurl().getSpec()),
                "Omnibox kept the previous tab's URL after switching tabs while the"
                        + " same-document debounce was latched.");
    }

    private void assertDisplayAndEditText(
            ToolbarDataProvider dataProvider, String displayText, String editText) {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    UrlBarData urlBarData = dataProvider.getUrlBarData();
                    assertEquals(
                            "Display text did not match",
                            displayText,
                            urlBarData.displayText.toString());
                    assertEquals("Editing text did not match", editText, urlBarData.editingText);
                });
    }

    /**
     * @param activity A reference to {@link ChromeTabbedActivity} to pull {@link android.view.View}
     *     data from.
     * @return The id of the current {@link Tab} as far as the {@link LocationBarModel} sees it.
     */
    public static int getCurrentTabId(final ChromeTabbedActivity activity) {
        ToolbarLayout toolbar = activity.findViewById(R.id.toolbar);
        Assert.assertNotNull("Toolbar is null", toolbar);

        ToolbarDataProvider dataProvider = toolbar.getToolbarDataProvider();
        Tab tab = dataProvider.getTab();
        return tab != null ? tab.getId() : Tab.INVALID_TAB_ID;
    }

    private static class TestLocationBarModel extends LocationBarModel {
        public TestLocationBarModel(Context context) {
            super(
                    context,
                    NewTabPageDelegate.EMPTY,
                    DomDistillerTabUtils::getFormattedUrlFromOriginalDistillerUrl,
                    new LocationBarModel.OfflineStatus() {},
                    ObservableSuppliers.createNonNull(ControlsPosition.TOP));
            initializeWithNative();

            Tab tab =
                    new MockTab(0, ProfileManager.getLastUsedRegularProfile()) {
                        @Override
                        public boolean isInitialized() {
                            return true;
                        }

                        @Override
                        public boolean isFrozen() {
                            return false;
                        }
                    };
            setTab(tab, tab.getProfile());
        }

        private void setVisibleGurl(GURL gurl) {
            mVisibleGurl = gurl;
        }

        private void setFullUrl(String fullUrl) {
            mFormattedFullUrl = fullUrl;
        }

        private void setDisplayUrl(String displayUrl) {
            mUrlForDisplay = displayUrl;
        }

        @Override
        public GURL getCurrentGurl() {
            return mVisibleGurl == null ? super.getCurrentGurl() : mVisibleGurl;
        }

        @Override
        public String calculateFormattedFullUrl() {
            return mFormattedFullUrl == null
                    ? super.calculateFormattedFullUrl()
                    : mFormattedFullUrl;
        }

        @Override
        public String calculateUrlForDisplay() {
            return mUrlForDisplay == null ? super.calculateUrlForDisplay() : mUrlForDisplay;
        }
    }
}
