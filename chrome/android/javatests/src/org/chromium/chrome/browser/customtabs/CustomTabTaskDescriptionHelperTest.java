// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;

import static org.junit.Assert.assertEquals;

import android.app.Activity;
import android.app.ActivityManager;
import android.content.Context;
import android.content.Intent;
import android.graphics.Color;

import androidx.annotation.NonNull;
import androidx.test.filters.MediumTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.app.ChromeActivity;
import org.chromium.chrome.browser.browserservices.intents.WebappConstants;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.webapps.WebappActivity;
import org.chromium.chrome.browser.webapps.WebappActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.R;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.chrome.test.util.browser.ThemeTestUtils;
import org.chromium.net.test.EmbeddedTestServer;
import org.chromium.ui.base.DeviceFormFactor;

/** Tests for {@link CustomTabTaskDescriptionHelper}. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class CustomTabTaskDescriptionHelperTest {
    @Rule public WebappActivityTestRule mWebappActivityTestRule = new WebappActivityTestRule();

    private EmbeddedTestServer mTestServer;

    @Before
    public void setUp() {
        mTestServer = mWebappActivityTestRule.getTestServer();
    }

    /**
     * Launches WebappActivity on webpage which provides red custom theme color. Waits till the
     * tab's theme color switches to red.
     */
    private WebappActivity launchWebappOnPageWithRedThemeColor(Intent launchIntent)
            throws Exception {
        String pageWithThemeColor =
                mTestServer.getURL("/chrome/test/data/android/theme_color_test.html");
        WebappActivity webappActivity =
                launchWebappAndWaitTillPageLoaded(launchIntent, pageWithThemeColor);
        ThemeTestUtils.waitForThemeColor(webappActivity, Color.RED);
        return webappActivity;
    }

    /**
     * Tests that the task description gives preference to the theme color value provided by the web
     * page when both the web page and the launch intent provide a custom theme color.
     */
    @Test
    @MediumTest
    @Restriction({DeviceFormFactor.PHONE})
    public void testPageHasThemeColorThemeColorInIntent() throws Exception {
        final int intentThemeColor = Color.GREEN;

        Intent launchIntent = mWebappActivityTestRule.createIntent();
        launchIntent.putExtra(WebappConstants.EXTRA_THEME_COLOR, (long) intentThemeColor);

        WebappActivity webappActivity = launchWebappOnPageWithRedThemeColor(launchIntent);
        assertEquals(Color.RED, fetchTaskDescriptionColor(webappActivity));
    }

    /**
     * Tests that the task description falls back to using the custom theme color from the launch
     * intent when the custom theme color is not provided by the webpage.
     */
    @Test
    @MediumTest
    @Restriction({DeviceFormFactor.PHONE})
    public void testPageNoThemeColorThemeColorInIntent() throws Exception {
        final int intentThemeColor = Color.GREEN;
        final String pageWithoutThemeColorUrl =
                mTestServer.getURL("/chrome/test/data/android/simple.html");

        Intent launchIntent = mWebappActivityTestRule.createIntent();
        launchIntent.putExtra(WebappConstants.EXTRA_THEME_COLOR, (long) intentThemeColor);
        WebappActivity webappActivity = launchWebappOnPageWithRedThemeColor(launchIntent);
        assertEquals(Color.RED, fetchTaskDescriptionColor(webappActivity));

        mWebappActivityTestRule.loadUrl(pageWithoutThemeColorUrl);
        ThemeTestUtils.waitForThemeColor(webappActivity, intentThemeColor);
        assertEquals(intentThemeColor, fetchTaskDescriptionColor(webappActivity));
    }

    /**
     * Tests that the task description uses R.color.default_primary_color when neither the webpage
     * nor the launch intent provides a custom theme color.
     */
    @Test
    @MediumTest
    @Restriction({DeviceFormFactor.PHONE})
    public void testPageNorLaunchIntentProvidesThemeColor() throws Exception {
        final String pageWithoutThemeColorUrl =
                mTestServer.getURL("/chrome/test/data/android/simple.html");

        Intent launchIntent = mWebappActivityTestRule.createIntent();
        WebappActivity webappActivity = launchWebappOnPageWithRedThemeColor(launchIntent);
        assertEquals(Color.RED, fetchTaskDescriptionColor(webappActivity));

        mWebappActivityTestRule.loadUrl(pageWithoutThemeColorUrl);
        int defaultThemeColor = computeDefaultThemeColor(webappActivity);
        ThemeTestUtils.waitForThemeColor(webappActivity, defaultThemeColor);
        int defaultTaskDescriptionColor = webappActivity.getColor(R.color.default_primary_color);
        assertEquals(defaultTaskDescriptionColor, fetchTaskDescriptionColor(webappActivity));
    }

    /**
     * Tests that the launch intent theme color is made opaque prior to being used for the task
     * description.
     */
    @Test
    @MediumTest
    @Restriction({DeviceFormFactor.PHONE})
    public void testLaunchIntentThemeColorMadeOpaque() throws Exception {
        final int intentThemeColor = Color.argb(100, 0, 255, 0);
        final int opaqueIntentThemeColor = Color.GREEN;
        final String pageWithoutThemeColorUrl =
                mTestServer.getURL("/chrome/test/data/android/simple.html");

        Intent launchIntent = mWebappActivityTestRule.createIntent();
        launchIntent.putExtra(WebappConstants.EXTRA_THEME_COLOR, (long) intentThemeColor);
        WebappActivity webappActivity = launchWebappOnPageWithRedThemeColor(launchIntent);
        assertEquals(Color.RED, fetchTaskDescriptionColor(webappActivity));

        mWebappActivityTestRule.loadUrl(pageWithoutThemeColorUrl);
        ThemeTestUtils.waitForThemeColor(webappActivity, opaqueIntentThemeColor);
        assertEquals(opaqueIntentThemeColor, fetchTaskDescriptionColor(webappActivity));
    }

    /**
     * Tests that the short_name is used in the task description when it is provided by the the
     * launch intent.
     */
    @Test
    @MediumTest
    public void testTitleInIntent() throws Exception {
        final String url = mTestServer.getURL("/chrome/test/data/android/simple.html");
        final String pageTitle = "Activity test page";
        final String intentShortName = "Intent Short Name";

        Intent launchIntent = mWebappActivityTestRule.createIntent();
        launchIntent.putExtra(WebappConstants.EXTRA_SHORT_NAME, intentShortName);
        launchIntent.removeExtra(WebappConstants.EXTRA_NAME);
        WebappActivity webappActivity = launchWebappAndWaitTillPageLoaded(launchIntent, url);

        waitForTitle(webappActivity.getActivityTab(), pageTitle);
        assertEquals(intentShortName, fetchTaskDescriptionLabel(webappActivity));
    }

    /**
     * Tests that the page title is used in the task description if the launch intent provides
     * neither a name nor a short_name.
     */
    @Test
    @MediumTest
    public void testNoTitleInIntent() throws Exception {
        final String url = mTestServer.getURL("/chrome/test/data/android/simple.html");
        final String pageTitle = "Activity test page";

        Intent launchIntent = mWebappActivityTestRule.createIntent();
        launchIntent.removeExtra(WebappConstants.EXTRA_NAME);
        launchIntent.removeExtra(WebappConstants.EXTRA_SHORT_NAME);
        WebappActivity webappActivity = launchWebappAndWaitTillPageLoaded(launchIntent, url);

        waitForTitle(webappActivity.getActivityTab(), pageTitle);
        assertEquals(pageTitle, fetchTaskDescriptionLabel(webappActivity));
    }

    private WebappActivity launchWebappAndWaitTillPageLoaded(Intent launchIntent, String url) {
        launchIntent.putExtra(WebappConstants.EXTRA_URL, url);
        mWebappActivityTestRule.startWebappActivity(launchIntent);
        return mWebappActivityTestRule.getActivity();
    }

    private void waitForTitle(Tab tab, String expectedTitle) throws Exception {
        if (ChromeTabUtils.getTitleOnUiThread(tab).equals(expectedTitle)) return;

        ChromeTabUtils.waitForTitle(tab, expectedTitle);
    }

    private int computeDefaultThemeColor(@NonNull ChromeActivity activity) throws Exception {
        return ThreadUtils.runOnUiThreadBlocking(
                () -> ThemeTestUtils.getDefaultThemeColor(activity.getActivityTab()));
    }

    /** Fetches the task description color from the ActivityManager. */
    private static int fetchTaskDescriptionColor(Activity activity) throws Exception {
        ActivityManager.TaskDescription taskDescription =
                (ActivityManager.TaskDescription) fetchTaskDescription(activity);
        return (taskDescription == null) ? Color.TRANSPARENT : taskDescription.getPrimaryColor();
    }

    /** Fetches the task description label from the ActivityManager. */
    private static String fetchTaskDescriptionLabel(Activity activity) throws Exception {
        ActivityManager.TaskDescription taskDescription =
                (ActivityManager.TaskDescription) fetchTaskDescription(activity);
        return (taskDescription == null) ? null : taskDescription.getLabel();
    }

    private static Object fetchTaskDescription(Activity activity) throws Exception {
        return ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    try {
                        ActivityManager activityManager =
                                (ActivityManager)
                                        activity.getSystemService(Context.ACTIVITY_SERVICE);
                        for (ActivityManager.AppTask task : activityManager.getAppTasks()) {
                            if (activity.getTaskId() == task.getTaskInfo().id) {
                                ActivityManager.RecentTaskInfo taskInfo = task.getTaskInfo();
                                return (taskInfo == null)
                                        ? null
                                        : (Object) taskInfo.taskDescription;
                            }
                        }
                    } catch (Exception e) {
                    }
                    return null;
                });
    }
}
