// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar;

import static org.junit.Assert.assertEquals;
import static org.mockito.Mockito.atLeast;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import android.support.test.InstrumentationRegistry;

import androidx.test.filters.MediumTest;
import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.params.ParameterAnnotations;
import org.chromium.base.test.params.ParameterProvider;
import org.chromium.base.test.params.ParameterSet;
import org.chromium.base.test.params.ParameterizedRunner;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.UrlUtils;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.dom_distiller.DomDistillerTabUtils;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.omnibox.LocationBarDataProvider;
import org.chromium.chrome.browser.omnibox.NewTabPageDelegate;
import org.chromium.chrome.browser.omnibox.SearchEngineLogoUtils;
import org.chromium.chrome.browser.omnibox.UrlBarData;
import org.chromium.chrome.browser.tab.MockTab;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabSelectionType;
import org.chromium.chrome.browser.toolbar.top.ToolbarLayout;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

/**
 * Tests for LocationBarModel.
 */
@RunWith(ParameterizedRunner.class)
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
        Assert.assertNotSame("No current tab", Tab.INVALID_TAB_ID,
                getCurrentTabId(mActivityTestRule.getActivity()));
        ChromeTabUtils.closeCurrentTab(
                InstrumentationRegistry.getInstrumentation(), mActivityTestRule.getActivity());
        assertEquals("Didn't close all tabs.", 0,
                ChromeTabUtils.getNumOpenTabs(mActivityTestRule.getActivity()));
        assertEquals("LocationBarModel is still trying to show a tab.", Tab.INVALID_TAB_ID,
                getCurrentTabId(mActivityTestRule.getActivity()));
    }

    @Test
    @SmallTest
    public void testDisplayAndEditText() {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            TestLocationBarModel model = new TestLocationBarModel();
            model.mUrl = UrlConstants.NTP_URL;
            assertDisplayAndEditText(model, "", null);

            model.mUrl = "chrome://about";
            model.mDisplayUrl = "chrome://about";
            model.mFullUrl = "chrome://about";
            assertDisplayAndEditText(model, "chrome://about", "chrome://about");

            model.mUrl = "https://www.foo.com";
            model.mDisplayUrl = "https://foo.com";
            model.mFullUrl = "https://foo.com";
            assertDisplayAndEditText(model, "https://foo.com", "https://foo.com");

            model.mUrl = "https://www.foo.com";
            model.mDisplayUrl = "foo.com";
            model.mFullUrl = "https://foo.com";
            assertDisplayAndEditText(model, "foo.com", "https://foo.com");
        });
    }

    /** Provides parameters for different types of transitions between tabs. */
    public static class IncognitoTransitionParamProvider implements ParameterProvider {
        @Override
        public Iterable<ParameterSet> getParameters() {
            List<ParameterSet> result = new ArrayList<>(8);
            for (boolean fromIncognito : Arrays.asList(true, false)) {
                for (boolean toIncognito : Arrays.asList(true, false)) {
                    result.add(new ParameterSet()
                                       .value(fromIncognito, toIncognito)
                                       .name(String.format(
                                               "from_%b_to_%b", fromIncognito, toIncognito)));
                }
            }
            return result;
        }
    }

    @Test
    @MediumTest
    @ParameterAnnotations.UseMethodParameter(IncognitoTransitionParamProvider.class)
    public void testOnIncognitoStateChange_switchTab(boolean fromIncognito, boolean toIncognito) {
        // Add a regular tab next to the one created in setup.
        mActivityTestRule.loadUrlInNewTab("about:blank", /*incognito=*/false);
        // Add two incognito tabs.
        mActivityTestRule.loadUrlInNewTab("about:blank", /*incognito=*/true);
        mActivityTestRule.loadUrlInNewTab("about:blank", /*incognito=*/true);

        ChromeTabbedActivity activity = mActivityTestRule.getActivity();
        LocationBarModel locationBarModel =
                activity.getToolbarManager().getLocationBarModelForTesting();
        LocationBarDataProvider.Observer observer = mock(LocationBarDataProvider.Observer.class);
        doAnswer((invocation) -> {
            assertEquals(toIncognito, locationBarModel.isIncognito());
            return null;
        })
                .when(observer)
                .onIncognitoStateChanged();

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mActivityTestRule.getActivity().getTabModelSelector().selectModel(fromIncognito);
            locationBarModel.addObserver(observer);

            // Switch to an existing tab.
            mActivityTestRule.getActivity().getTabModelSelector().selectModel(/*incognito=*/
                    toIncognito);
            mActivityTestRule.getActivity().getTabModelSelector().getCurrentModel().setIndex(
                    0, TabSelectionType.FROM_USER);
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
        mActivityTestRule.loadUrlInNewTab("about:blank", /*incognito=*/false);
        // Add two incognito tabs.
        mActivityTestRule.loadUrlInNewTab("about:blank", /*incognito=*/true);
        mActivityTestRule.loadUrlInNewTab("about:blank", /*incognito=*/true);

        ChromeTabbedActivity activity = mActivityTestRule.getActivity();
        LocationBarModel locationBarModel =
                activity.getToolbarManager().getLocationBarModelForTesting();
        LocationBarDataProvider.Observer observer = mock(LocationBarDataProvider.Observer.class);
        doAnswer((invocation) -> {
            assertEquals(toIncognito, locationBarModel.isIncognito());
            return null;
        })
                .when(observer)
                .onIncognitoStateChanged();

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mActivityTestRule.getActivity().getTabModelSelector().selectModel(fromIncognito);
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
        TestThreadUtils.runOnUiThreadBlocking(() -> { locationBarModel.addObserver(observer); });

        mActivityTestRule.loadUrl(UrlUtils.encodeHtmlDataUri("test content"));

        verify(observer, atLeast(1)).onSecurityStateChanged();
    }

    private void assertDisplayAndEditText(
            ToolbarDataProvider dataProvider, String displayText, String editText) {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            UrlBarData urlBarData = dataProvider.getUrlBarData();
            assertEquals(
                    "Display text did not match", displayText, urlBarData.displayText.toString());
            assertEquals("Editing text did not match", editText, urlBarData.editingText);
        });
    }

    /**
     * @param activity A reference to {@link ChromeTabbedActivity} to pull
     *            {@link android.view.View} data from.
     * @return The id of the current {@link Tab} as far as the {@link LocationBarModel} sees it.
     */
    public static int getCurrentTabId(final ChromeTabbedActivity activity) {
        ToolbarLayout toolbar = (ToolbarLayout) activity.findViewById(R.id.toolbar);
        Assert.assertNotNull("Toolbar is null", toolbar);

        ToolbarDataProvider dataProvider = toolbar.getToolbarDataProvider();
        Tab tab = dataProvider.getTab();
        return tab != null ? tab.getId() : Tab.INVALID_TAB_ID;
    }

    private class TestLocationBarModel extends LocationBarModel {
        private String mDisplayUrl;
        private String mFullUrl;
        private String mUrl;

        public TestLocationBarModel() {
            // clang-format off
            super(ContextUtils.getApplicationContext(), NewTabPageDelegate.EMPTY,
                    DomDistillerTabUtils::getFormattedUrlFromOriginalDistillerUrl,
                    window -> null, new LocationBarModel.OfflineStatus() {},
                    SearchEngineLogoUtils.getInstance());
            // clang-format on
            initializeWithNative();

            Tab tab = new MockTab(0, false) {
                @Override
                public boolean isInitialized() {
                    return true;
                }

                @Override
                public boolean isFrozen() {
                    return false;
                }
            };
            setTab(tab, false);
        }

        @Override
        public String getCurrentUrl() {
            return mUrl == null ? super.getCurrentUrl() : mUrl;
        }

        @Override
        public String getFormattedFullUrl() {
            return mFullUrl == null ? super.getFormattedFullUrl() : mFullUrl;
        }

        @Override
        public String getUrlForDisplay() {
            return mDisplayUrl == null ? super.getUrlForDisplay() : mDisplayUrl;
        }
    }
}
