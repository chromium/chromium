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

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.params.ParameterAnnotations;
import org.chromium.base.test.params.ParameterProvider;
import org.chromium.base.test.params.ParameterSet;
import org.chromium.base.test.params.ParameterizedRunner;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.UrlUtils;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.dom_distiller.DomDistillerTabUtils;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.omnibox.LocationBarDataProvider;
import org.chromium.chrome.browser.omnibox.NewTabPageDelegate;
import org.chromium.chrome.browser.omnibox.UrlBarData;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.browser.tab.MockTab;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabSelectionType;
import org.chromium.chrome.browser.toolbar.top.ToolbarLayout;
import org.chromium.chrome.test.ChromeJUnit4RunnerDelegate;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.R;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.url.GURL;
import org.chromium.url.JUnitTestGURLs;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import java.util.concurrent.atomic.AtomicReference;

/** Tests for LocationBarModel. */
@RunWith(ParameterizedRunner.class)
@ParameterAnnotations.UseRunnerDelegate(ChromeJUnit4RunnerDelegate.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class LocationBarModelTest {
    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    @Before
    public void setUp() throws InterruptedException {
        mActivityTestRule.startMainActivityOnBlankPage();
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
                    model.setVisibleGurl(UrlConstants.ntpGurl());
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

                    // https://crbug.com/1214481
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
        ToolbarDataProvider.Observer observer =
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
                    locationBarModel.addToolbarDataProviderObserver(observer);

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
    public void testOnIncognitoStateChange_switchTab(boolean fromIncognito, boolean toIncognito) {
        // Add a regular tab next to the one created in setup.
        mActivityTestRule.loadUrlInNewTab("about:blank", /* incognito= */ false);
        // Add two incognito tabs.
        mActivityTestRule.loadUrlInNewTab("about:blank", /* incognito= */ true);
        mActivityTestRule.loadUrlInNewTab("about:blank", /* incognito= */ true);

        ChromeTabbedActivity activity = mActivityTestRule.getActivity();
        LocationBarModel locationBarModel =
                activity.getToolbarManager().getLocationBarModelForTesting();
        LocationBarDataProvider.Observer observer = mock(LocationBarDataProvider.Observer.class);
        doAnswer(
                        (invocation) -> {
                            assertEquals(toIncognito, locationBarModel.isIncognito());
                            return null;
                        })
                .when(observer)
                .onIncognitoStateChanged();

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mActivityTestRule
                            .getActivity()
                            .getTabModelSelector()
                            .selectModel(fromIncognito);
                    locationBarModel.addObserver(observer);

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
            verify(observer).onIncognitoStateChanged();
        } else {
            verify(observer, times(0)).onIncognitoStateChanged();
        }
    }

    @Test
    @MediumTest
    @ParameterAnnotations.UseMethodParameter(IncognitoTransitionParamProvider.class)
    public void testOnIncognitoStateChange_newTab(boolean fromIncognito, boolean toIncognito) {
        // Add a regular tab next to the one created in setup.
        mActivityTestRule.loadUrlInNewTab("about:blank", /* incognito= */ false);
        // Add two incognito tabs.
        mActivityTestRule.loadUrlInNewTab("about:blank", /* incognito= */ true);
        mActivityTestRule.loadUrlInNewTab("about:blank", /* incognito= */ true);

        ChromeTabbedActivity activity = mActivityTestRule.getActivity();
        LocationBarModel locationBarModel =
                activity.getToolbarManager().getLocationBarModelForTesting();
        LocationBarDataProvider.Observer observer = mock(LocationBarDataProvider.Observer.class);
        doAnswer(
                        (invocation) -> {
                            assertEquals(toIncognito, locationBarModel.isIncognito());
                            return null;
                        })
                .when(observer)
                .onIncognitoStateChanged();

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mActivityTestRule
                            .getActivity()
                            .getTabModelSelector()
                            .selectModel(fromIncognito);
                    locationBarModel.addObserver(observer);
                });

        // Switch to a new tab.
        mActivityTestRule.loadUrlInNewTab("about:blank", toIncognito);

        assertEquals(toIncognito, locationBarModel.isIncognito());
        if (fromIncognito != toIncognito) {
            verify(observer).onIncognitoStateChanged();
        } else {
            verify(observer, times(0)).onIncognitoStateChanged();
        }
    }

    @Test
    @MediumTest
    public void testOnSecurityStateChanged() {
        LocationBarModel locationBarModel =
                mActivityTestRule.getActivity().getToolbarManager().getLocationBarModelForTesting();
        LocationBarDataProvider.Observer observer = mock(LocationBarDataProvider.Observer.class);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    locationBarModel.addObserver(observer);
                });

        mActivityTestRule.loadUrl(UrlUtils.encodeHtmlDataUri("test content"));

        verify(observer, atLeast(1)).onSecurityStateChanged();
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
                    new LocationBarModel.OfflineStatus() {});
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
