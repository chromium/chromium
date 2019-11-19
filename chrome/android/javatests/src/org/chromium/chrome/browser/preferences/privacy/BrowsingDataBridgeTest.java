// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.preferences.privacy;

import static org.junit.Assert.assertThat;

import android.support.test.filters.SmallTest;

import org.hamcrest.Matchers;
import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.RetryOnFailure;
import org.chromium.base.test.util.UserActionTester;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.browsing_data.BrowsingDataType;
import org.chromium.chrome.browser.browsing_data.TimePeriod;
import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

import java.util.ArrayList;
import java.util.Iterator;
import java.util.List;

/**
 * Integration tests for ClearBrowsingDataPreferences.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class BrowsingDataBridgeTest {
    @Rule
    public ChromeActivityTestRule<ChromeActivity> mActivityTestRule =
            new ChromeActivityTestRule<>(ChromeActivity.class);

    private CallbackHelper mCallbackHelper;
    private BrowsingDataBridge.OnClearBrowsingDataListener mListener;
    private UserActionTester mActionTester;

    @Before
    public void setUp() throws Exception {
        mCallbackHelper = new CallbackHelper();
        mListener = new BrowsingDataBridge.OnClearBrowsingDataListener() {
            @Override
            public void onBrowsingDataCleared() {
                mCallbackHelper.notifyCalled();
            }
        };
        mActivityTestRule.startMainActivityOnBlankPage();
        mActionTester = new UserActionTester();
    }

    @After
    public void tearDown() {
        mActionTester.tearDown();
    }

    /**
     * Test no clear browsing data calls.
     */
    @Test
    @SmallTest
    @RetryOnFailure
    public void testNoCalls() throws Exception {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            BrowsingDataBridge.getInstance().clearBrowsingData(
                    mListener, new int[] {}, TimePeriod.ALL_TIME);
        });
        mCallbackHelper.waitForCallback(0);
        assertThat(mActionTester.toString(), getActions(),
                Matchers.contains("ClearBrowsingData_Everything"));
    }

    /**
     * Test cookies deletion.
     */
    @Test
    @SmallTest
    public void testCookiesDeleted() throws Exception {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            BrowsingDataBridge.getInstance().clearBrowsingData(
                    mListener, new int[] {BrowsingDataType.COOKIES}, TimePeriod.LAST_HOUR);
        });
        mCallbackHelper.waitForCallback(0);
        assertThat(mActionTester.toString(), getActions(),
                Matchers.containsInAnyOrder("ClearBrowsingData_LastHour",
                        "ClearBrowsingData_MaskContainsUnprotectedWeb", "ClearBrowsingData_Cookies",
                        "ClearBrowsingData_SiteUsageData", "ClearBrowsingData_ContentLicenses"));
    }

    /**
     * Get ClearBrowsingData related actions, filter all other actions to avoid flakes.
     */
    private List<String> getActions() {
        List<String> actions = new ArrayList<>(mActionTester.getActions());
        Iterator<String> it = actions.iterator();
        while (it.hasNext()) {
            if (!it.next().startsWith("ClearBrowsingData_")) {
                it.remove();
            }
        }
        return actions;
    }

    /**
     * Test history deletion.
     */
    @Test
    @SmallTest
    public void testHistoryDeleted() throws Exception {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            BrowsingDataBridge.getInstance().clearBrowsingData(
                    mListener, new int[] {BrowsingDataType.HISTORY}, TimePeriod.LAST_DAY);
        });
        mCallbackHelper.waitForCallback(0);
        assertThat(mActionTester.toString(), getActions(),
                Matchers.containsInAnyOrder("ClearBrowsingData_LastDay",
                        "ClearBrowsingData_MaskContainsUnprotectedWeb",
                        "ClearBrowsingData_History"));
    }

    /**
     * Test deleting cache and content settings.
     */
    @Test
    @SmallTest
    public void testClearingSiteSettingsAndCache() throws Exception {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            BrowsingDataBridge.getInstance().clearBrowsingData(mListener,
                    new int[] {
                            BrowsingDataType.CACHE,
                            BrowsingDataType.SITE_SETTINGS,
                    },
                    TimePeriod.FOUR_WEEKS);
        });
        mCallbackHelper.waitForCallback(0);
        assertThat(mActionTester.toString(), getActions(),
                Matchers.containsInAnyOrder("ClearBrowsingData_LastMonth",
                        "ClearBrowsingData_MaskContainsUnprotectedWeb", "ClearBrowsingData_Cache",
                        "ClearBrowsingData_ShaderCache", "ClearBrowsingData_ContentSettings"));
    }

    /**
     * Test deleting cache and content settings with important sites.
     */
    @Test
    @SmallTest
    public void testClearingSiteSettingsAndCacheWithImportantSites() throws Exception {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            BrowsingDataBridge.getInstance().clearBrowsingDataExcludingDomains(mListener,
                    new int[] {
                            BrowsingDataType.CACHE,
                            BrowsingDataType.SITE_SETTINGS,
                    },
                    TimePeriod.FOUR_WEEKS, new String[] {"google.com"}, new int[] {1},
                    new String[0], new int[0]);
        });
        mCallbackHelper.waitForCallback(0);
        assertThat(mActionTester.toString(), getActions(),
                Matchers.containsInAnyOrder("ClearBrowsingData_LastMonth",
                        // ClearBrowsingData_MaskContainsUnprotectedWeb is logged
                        // twice because important storage is deleted separately.
                        "ClearBrowsingData_MaskContainsUnprotectedWeb",
                        "ClearBrowsingData_MaskContainsUnprotectedWeb", "ClearBrowsingData_Cache",
                        "ClearBrowsingData_ShaderCache", "ClearBrowsingData_ContentSettings"));
    }

    /**
     * Test deleting all browsing data. (Except bookmarks, they are deleted in Java code)
     */
    @Test
    @SmallTest
    public void testClearingAll() throws Exception {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            BrowsingDataBridge.getInstance().clearBrowsingData(mListener,
                    new int[] {
                            BrowsingDataType.CACHE,
                            BrowsingDataType.COOKIES,
                            BrowsingDataType.FORM_DATA,
                            BrowsingDataType.HISTORY,
                            BrowsingDataType.PASSWORDS,
                            BrowsingDataType.SITE_SETTINGS,
                    },
                    TimePeriod.LAST_WEEK);
        });
        mCallbackHelper.waitForCallback(0);
        assertThat(mActionTester.toString(), getActions(),
                Matchers.containsInAnyOrder("ClearBrowsingData_LastWeek",
                        "ClearBrowsingData_MaskContainsUnprotectedWeb", "ClearBrowsingData_Cache",
                        "ClearBrowsingData_ShaderCache", "ClearBrowsingData_Cookies",
                        "ClearBrowsingData_Autofill", "ClearBrowsingData_History",
                        "ClearBrowsingData_Passwords", "ClearBrowsingData_ContentSettings",
                        "ClearBrowsingData_SiteUsageData", "ClearBrowsingData_ContentLicenses"));
    }
}
