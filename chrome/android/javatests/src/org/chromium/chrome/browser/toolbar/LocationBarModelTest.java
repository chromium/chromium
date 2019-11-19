// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar;

import android.support.test.InstrumentationRegistry;
import android.support.test.filters.MediumTest;
import android.support.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.RetryOnFailure;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.omnibox.UrlBarData;
import org.chromium.chrome.browser.tab.MockTab;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.toolbar.top.ToolbarLayout;
import org.chromium.chrome.browser.util.UrlConstants;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

/**
 * Tests for LocationBarModel.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
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
    @RetryOnFailure
    public void testClosingLastTabReflectedInModel() {
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

    private class TestLocationBarModel extends LocationBarModel {
        private String mDisplayUrl;
        private String mFullUrl;
        private String mUrl;

        public TestLocationBarModel() {
            super(ContextUtils.getApplicationContext());
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
