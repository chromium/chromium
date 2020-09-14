// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import android.content.Intent;
import android.support.test.InstrumentationRegistry;

import androidx.test.filters.MediumTest;

import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.customtabs.CustomTabActivityTestRule;
import org.chromium.chrome.browser.customtabs.CustomTabsTestUtils;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.incognito.IncognitoDataTestUtils;
import org.chromium.chrome.browser.omnibox.UrlBarData;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.MockTab;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.toolbar.top.ToolbarLayout;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

import java.util.concurrent.TimeoutException;

/**
 * Tests for LocationBarModel.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class LocationBarModelTest {
    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    @Rule
    public CustomTabActivityTestRule mCustomTabActivityTestRule = new CustomTabActivityTestRule();

    /**
     * After closing all {@link Tab}s, the {@link LocationBarModel} should know that it is not
     * showing any {@link Tab}.
     */
    @Test
    @Feature({"Android-Toolbar"})
    @MediumTest
    public void testClosingLastTabReflectedInModel() {
        mActivityTestRule.startMainActivityOnBlankPage();
        Assert.assertNotSame("No current tab", Tab.INVALID_TAB_ID,
                getCurrentTabId(mActivityTestRule.getActivity()));
        ChromeTabUtils.closeCurrentTab(
                InstrumentationRegistry.getInstrumentation(), mActivityTestRule.getActivity());
        Assert.assertEquals("Didn't close all tabs.", 0,
                ChromeTabUtils.getNumOpenTabs(mActivityTestRule.getActivity()));
        Assert.assertEquals("LocationBarModel is still trying to show a tab.", Tab.INVALID_TAB_ID,
                getCurrentTabId(mActivityTestRule.getActivity()));
    }

    @Test
    @MediumTest
    public void testDisplayAndEditText() {
        mActivityTestRule.startMainActivityOnBlankPage();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            boolean incognito = false;
            TestLocationBarModel model = new TestLocationBarModel(getMockTab(incognito), incognito);
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

    @Test
    @MediumTest
    @DisabledTest(message = "crbug/1128073")
    public void testGetProfileOnNullTabInIncognito() {
        mActivityTestRule.startMainActivityOnBlankPage();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            boolean incognito = true;
            TestLocationBarModel model = new TestLocationBarModel(null, incognito);
            Profile profile = model.getProfile();
            assertTrue(profile.isPrimaryOTRProfile());
        });
    }

    @Test
    @MediumTest
    @DisabledTest(message = "crbug/1128073")
    public void testGetProfileOnMockTabInIncognito() {
        mActivityTestRule.startMainActivityOnBlankPage();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            boolean incognito = true;
            TestLocationBarModel model = new TestLocationBarModel(getMockTab(incognito), incognito);
            Profile profile = model.getProfile();
            assertTrue(profile.isPrimaryOTRProfile());
        });
    }

    @Test
    @MediumTest
    @Features.EnableFeatures({ChromeFeatureList.CCT_INCOGNITO})
    public void testGetProfileOnMockTabInIncognitoCCT() throws TimeoutException {
        IncognitoDataTestUtils.fireAndWaitForCctWarmup();

        // Create an launch an incognito CCT.
        Intent intent = CustomTabsTestUtils.createMinimalCustomTabIntent(
                InstrumentationRegistry.getContext(), "about:blank");
        intent.putExtra(IntentHandler.EXTRA_OPEN_NEW_INCOGNITO_TAB, true);
        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(intent);

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            boolean incognito = true;
            // Setup LocationBarModel
            Tab tab = mCustomTabActivityTestRule.getActivity().getActivityTab();
            TestLocationBarModel model = new TestLocationBarModel(tab, incognito);

            Profile profile = model.getProfile();
            assertFalse(profile.isPrimaryOTRProfile());
        });
    }

    @Test
    @MediumTest
    @DisabledTest(message = "crbug/1128073")
    public void testGetProfileOnMockTabInRegularCCT() {
        // Create an launch a regular CCT.
        Intent intent = CustomTabsTestUtils.createMinimalCustomTabIntent(
                InstrumentationRegistry.getContext(), "about:blank");
        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(intent);

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            boolean incognito = true;
            // Setup LocationBarModel
            Tab tab = mCustomTabActivityTestRule.getActivity().getActivityTab();
            TestLocationBarModel model = new TestLocationBarModel(tab, incognito);

            Profile profile = model.getProfile();
            assertTrue(profile.isPrimaryOTRProfile());
        });
    }

    private void assertDisplayAndEditText(
            ToolbarDataProvider dataProvider, String displayText, String editText) {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            UrlBarData urlBarData = dataProvider.getUrlBarData();
            Assert.assertEquals(
                    "Display text did not match", displayText, urlBarData.displayText.toString());
            Assert.assertEquals("Editing text did not match", editText, urlBarData.editingText);
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

    public static Tab getMockTab(boolean incognito) {
        Tab tab = new MockTab(0, incognito) {
            @Override
            public boolean isInitialized() {
                return true;
            }

            @Override
            public boolean isFrozen() {
                return false;
            }
        };
        return tab;
    }

    private class TestLocationBarModel extends LocationBarModel {
        private String mDisplayUrl;
        private String mFullUrl;
        private String mUrl;

        public TestLocationBarModel(Tab tab, boolean incognito) {
            super(ContextUtils.getApplicationContext());
            initializeWithNative();
            setTab(tab, incognito);
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
