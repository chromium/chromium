// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.incognito;

import static org.junit.Assert.assertEquals;

import androidx.test.core.app.ApplicationProvider;
import androidx.test.filters.LargeTest;

import org.hamcrest.Matchers;
import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.params.ParameterAnnotations.UseMethodParameter;
import org.chromium.base.test.params.ParameterAnnotations.UseRunnerDelegate;
import org.chromium.base.test.params.ParameterProvider;
import org.chromium.base.test.params.ParameterSet;
import org.chromium.base.test.params.ParameterizedRunner;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.chrome.browser.customtabs.IncognitoCustomTabActivityTestRule;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.incognito.IncognitoDataTestUtils.ActivityType;
import org.chromium.chrome.browser.incognito.IncognitoDataTestUtils.TestParams;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeJUnit4RunnerDelegate;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.content_public.browser.test.util.JavaScriptUtils;
import org.chromium.net.test.EmbeddedTestServer;

import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.TimeoutException;

/**
 * This test class checks cookie leakage between all different pairs of Activity types with a
 * constraint that one of the interacting activity must be either Incognito Tab or Incognito CCT.
 */
@RunWith(ParameterizedRunner.class)
@UseRunnerDelegate(ChromeJUnit4RunnerDelegate.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE, ChromeSwitches.DISABLE_ALL_IPH})
public class IncognitoCookieLeakageTest {
    private static final String COOKIES_SETTING_PATH = "/chrome/test/data/android/cookie.html";
    private String mCookiesTestPage;
    private EmbeddedTestServer mTestServer;

    @Rule
    public ChromeTabbedActivityTestRule mChromeActivityTestRule =
            new ChromeTabbedActivityTestRule();

    @Rule
    public IncognitoCustomTabActivityTestRule mCustomTabActivityTestRule =
            new IncognitoCustomTabActivityTestRule();

    @Before
    public void setUp() throws TimeoutException {
        mTestServer =
                EmbeddedTestServer.createAndStartServer(
                        ApplicationProvider.getApplicationContext());
        mCookiesTestPage = mTestServer.getURL(COOKIES_SETTING_PATH);
    }

    @After
    public void tearDown() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> IncognitoDataTestUtils.closeTabs(mChromeActivityTestRule));
    }

    private void setCookies(Tab tab) throws TimeoutException {
        CriteriaHelper.pollUiThread(
                () -> Criteria.checkThat(tab.getWebContents(), Matchers.notNullValue()));
        JavaScriptUtils.executeJavaScriptAndWaitForResult(tab.getWebContents(), "setCookie()");
        assertCookies(tab, "\"Foo=Bar\"");
    }

    private void assertCookies(Tab tab, String expected) throws TimeoutException {
        CriteriaHelper.pollUiThread(
                () -> Criteria.checkThat(tab.getWebContents(), Matchers.notNullValue()));
        String actual =
                JavaScriptUtils.executeJavaScriptAndWaitForResult(
                        tab.getWebContents(), "getCookie()");
        if (actual.equalsIgnoreCase("null")) actual = "\"\"";
        assertEquals(expected, actual);
    }

    /**
     * A class to provide the list of test parameters encapsulating Activity pairs, spliced on
     * regular and Incognito mode, where cookie shouldn't leak.
     */
    public static class IsolatedFlowsParams implements ParameterProvider {
        @Override
        public List<ParameterSet> getParameters() {
            List<ParameterSet> result = new ArrayList<>();
            result.addAll(new TestParams.RegularToIncognito().getParameters());
            result.addAll(new TestParams.IncognitoToRegular().getParameters());
            return result;
        }
    }

    // TODO(crbug.com/40107157) : Currently, incognito CCTs are not isolated and hence they share
    // the session with other incognito sessions. Once, they are properly isolated we should change
    // the test to expect that cookies are not leaked from/to an incognito CCT session.
    @Test
    @LargeTest
    @UseMethodParameter(TestParams.IncognitoToIncognito.class)
    public void testCookiesDoNotLeakFromIncognitoToIncognito(
            String incognitoActivityType1, String incognitoActivityType2) throws TimeoutException {
        ActivityType incognitoActivity1 = ActivityType.valueOf(incognitoActivityType1);
        ActivityType incognitoActivity2 = ActivityType.valueOf(incognitoActivityType2);

        Tab setter_tab =
                incognitoActivity1.launchUrl(
                        mChromeActivityTestRule, mCustomTabActivityTestRule, mCookiesTestPage);
        setCookies(setter_tab);

        Tab getter_tab =
                incognitoActivity2.launchUrl(
                        mChromeActivityTestRule, mCustomTabActivityTestRule, mCookiesTestPage);

        String expected = "\"\"";

        assertCookies(getter_tab, expected);
    }

    // This test cookie does not leak from regular to incognito and from incognito to regular
    // session across different activity types.
    @Test
    @LargeTest
    @UseMethodParameter(IsolatedFlowsParams.class)
    public void testCookiesDoNotLeakBetweenRegularAndIncognito(
            String setterActivityType, String getterActivityType) throws TimeoutException {
        ActivityType setterActivity = ActivityType.valueOf(setterActivityType);
        ActivityType getterActivity = ActivityType.valueOf(getterActivityType);

        Tab setter_tab =
                setterActivity.launchUrl(
                        mChromeActivityTestRule, mCustomTabActivityTestRule, mCookiesTestPage);
        setCookies(setter_tab);

        Tab getter_tab =
                getterActivity.launchUrl(
                        mChromeActivityTestRule, mCustomTabActivityTestRule, mCookiesTestPage);

        assertCookies(getter_tab, "\"\"");
    }
}
