// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps;

import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.support.test.InstrumentationRegistry;
import android.support.test.filters.MediumTest;
import android.view.View;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ApplicationStatus;
import org.chromium.base.ContextUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.RetryOnFailure;
import org.chromium.base.test.util.UrlUtils;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.DeferredStartupHandler;
import org.chromium.chrome.browser.ShortcutHelper;
import org.chromium.chrome.browser.document.ChromeIntentUtil;
import org.chromium.chrome.browser.tab.TabIdManager;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.MultiActivityTestRule;
import org.chromium.chrome.test.util.ApplicationTestUtils;
import org.chromium.chrome.test.util.browser.webapps.WebappTestHelper;
import org.chromium.content_public.browser.test.util.Criteria;
import org.chromium.content_public.browser.test.util.CriteriaHelper;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

/**
 * Tests that WebappActivities are launched correctly.
 *
 * This test seems a little wonky because WebappActivities launched differently, depending on what
 * OS the user is on.  Pre-L, WebappActivities were manually instanced and assigned by the
 * WebappManager.  On L and above, WebappActivities are automatically instanced by Android and the
 * FLAG_ACTIVITY_NEW_DOCUMENT mechanism.  Moreover, we don't have access to the task list pre-L so
 * we have to assume that any non-running WebappActivities are not listed in Android's Overview.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@RetryOnFailure
public class WebappModeTest {
    @Rule
    public MultiActivityTestRule mTestRule = new MultiActivityTestRule();

    private static final String WEBAPP_1_ID = "webapp_id_1";
    private static final String WEBAPP_1_URL = UrlUtils.encodeHtmlDataUri(
            "<html><head><title>Web app #1</title><meta name='viewport' "
            + "content='width=device-width initial-scale=0.5, maximum-scale=0.5'></head>"
            + "<body bgcolor='#011684'>Webapp 1</body></html>");
    private static final String WEBAPP_1_TITLE = "Web app #1";

    private static final String WEBAPP_2_ID = "webapp_id_2";
    private static final String WEBAPP_2_URL =
            UrlUtils.encodeHtmlDataUri("<html><body bgcolor='#840116'>Webapp 2</body></html>");
    private static final String WEBAPP_2_TITLE = "Web app #2";

    private static final String WEBAPP_ICON = "iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAIAAACQd1PeAAAACXB"
            + "IWXMAAAsTAAALEwEAmpwYAAAAB3RJTUUH3wQIFB4cxOfiSQAAABl0RVh0Q29tbWVudABDcmVhdGVkIHdpdG"
            + "ggR0lNUFeBDhcAAAAMSURBVAjXY2AUawEAALcAnI/TkI8AAAAASUVORK5CYII=";

    private Intent createIntent(String id, String url, String title, String icon, boolean addMac) {
        Intent intent = WebappTestHelper.createMinimalWebappIntent(id, url);
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        intent.setPackage(InstrumentationRegistry.getTargetContext().getPackageName());
        intent.setAction(WebappLauncherActivity.ACTION_START_WEBAPP);
        if (addMac) {
            // Needed for security reasons.  If the MAC is excluded, the URL of the webapp is opened
            // in a browser window, instead.
            String mac = ShortcutHelper.getEncodedMac(url);
            intent.putExtra(ShortcutHelper.EXTRA_MAC, mac);
        }

        intent.putExtra(ShortcutHelper.EXTRA_ICON, icon);
        intent.putExtra(ShortcutHelper.EXTRA_NAME, title);
        return intent;
    }

    private void fireWebappIntent(String id, String url, String title, String icon,
            boolean addMac) {
        Intent intent = createIntent(id, url, title, icon, addMac);

        InstrumentationRegistry.getTargetContext().startActivity(intent);
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();
        ApplicationTestUtils.waitUntilChromeInForeground();
    }

    @Before
    public void setUp() {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            WebappRegistry.refreshSharedPrefsForTesting();

            // Register the webapps so when the data storage is opened, the test doesn't crash.
            // There is no race condition with the retrieval as AsyncTasks are run sequentially on
            // the background thread.
            WebappRegistry.getInstance().register(
                    WEBAPP_1_ID, new WebappRegistry.FetchWebappDataStorageCallback() {
                        @Override
                        public void onWebappDataStorageRetrieved(WebappDataStorage storage) {
                            WebappInfo webappInfo = WebappInfo.create(createIntent(
                                    WEBAPP_1_ID, WEBAPP_1_URL, WEBAPP_1_TITLE, WEBAPP_ICON, true));
                            storage.updateFromWebappInfo(webappInfo);
                        }
                    });
            WebappRegistry.getInstance().register(
                    WEBAPP_2_ID, new WebappRegistry.FetchWebappDataStorageCallback() {
                        @Override
                        public void onWebappDataStorageRetrieved(WebappDataStorage storage) {
                            WebappInfo webappInfo = WebappInfo.create(createIntent(
                                    WEBAPP_1_ID, WEBAPP_1_URL, WEBAPP_1_TITLE, WEBAPP_ICON, true));
                            storage.updateFromWebappInfo(webappInfo);
                        }
                    });
        });
    }

    /**
     * Tests that WebappActivities are started properly.
     */
    @Test
    @MediumTest
    @Feature({"Webapps"})
    public void testWebappLaunches() {
        final WebappActivity firstActivity =
                startWebappActivity(WEBAPP_1_ID, WEBAPP_1_URL, WEBAPP_1_TITLE, WEBAPP_ICON);
        final int firstTabId = firstActivity.getActivityTab().getId();

        // Firing a different Intent should start a new WebappActivity instance.
        fireWebappIntent(WEBAPP_2_ID, WEBAPP_2_URL, WEBAPP_2_TITLE, WEBAPP_ICON, true);
        CriteriaHelper.pollUiThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                Activity lastActivity = ApplicationStatus.getLastTrackedFocusedActivity();
                if (!isWebappActivityReady(lastActivity)) return false;

                WebappActivity lastWebappActivity = (WebappActivity) lastActivity;
                return lastWebappActivity.getActivityTab().getId() != firstTabId;
            }
        });

        // Firing the first Intent should bring back the first WebappActivity instance, or at least
        // a WebappActivity with the same tab if the other one was killed by Android mid-test.
        fireWebappIntent(WEBAPP_1_ID, WEBAPP_1_URL, WEBAPP_1_TITLE, WEBAPP_ICON, true);
        CriteriaHelper.pollUiThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                Activity lastActivity = ApplicationStatus.getLastTrackedFocusedActivity();
                if (!isWebappActivityReady(lastActivity)) return false;

                WebappActivity lastWebappActivity = (WebappActivity) lastActivity;
                return lastWebappActivity.getActivityTab().getId() == firstTabId;
            }
        });
    }

    /**
     * Tests that the WebappActivity gets the next available Tab ID instead of 0.
     */
    @Test
    @MediumTest
    @Feature({"Webapps"})
    public void testWebappTabIdsProperlyAssigned() {
        SharedPreferences prefs = ContextUtils.getAppSharedPreferences();
        SharedPreferences.Editor editor = prefs.edit();
        editor.putInt(TabIdManager.PREF_NEXT_ID, 11684);
        editor.apply();

        final WebappActivity webappActivity =
                startWebappActivity(WEBAPP_1_ID, WEBAPP_1_URL, WEBAPP_1_TITLE, WEBAPP_ICON);
        Assert.assertEquals(
                "Wrong Tab ID was used", 11684, webappActivity.getActivityTab().getId());
    }

    /**
     * Tests that a WebappActivity can be brought forward by firing an Intent with
     * TabOpenType.BRING_TAB_TO_FRONT.
     */
    @Test
    @MediumTest
    @Feature({"Webapps"})
    public void testBringTabToFront() {
        // Start the WebappActivity.
        final WebappActivity firstActivity =
                startWebappActivity(WEBAPP_1_ID, WEBAPP_1_URL, WEBAPP_1_TITLE, WEBAPP_ICON);
        final int webappTabId = firstActivity.getActivityTab().getId();

        // Return home.
        final Context context = InstrumentationRegistry.getTargetContext();
        ApplicationTestUtils.fireHomeScreenIntent(context);
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();

        // Bring the WebappActivity back via an Intent.
        Intent intent = ChromeIntentUtil.createBringTabToFrontIntent(webappTabId);
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        context.startActivity(intent);

        // When Chrome is back in the foreground, confirm that the correct Activity was restored.
        // Because of Android killing Activities willy-nilly, it may not be the same Activity, but
        // it should have the same Tab ID.
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();
        ApplicationTestUtils.waitUntilChromeInForeground();
        CriteriaHelper.pollInstrumentationThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                Activity lastActivity = ApplicationStatus.getLastTrackedFocusedActivity();
                if (!isWebappActivityReady(lastActivity)) return false;

                WebappActivity webappActivity = (WebappActivity) lastActivity;
                return webappActivity.getActivityTab().getId() == webappTabId;
            }
        });
    }

    /**
     * Ensure WebappActivities can't be launched without proper security checks.
     */
    @Test
    //@MediumTest
    //@Feature({"Webapps"})
    @DisabledTest(message = "crbug.com/755114")
    public void testWebappRequiresValidMac() throws Exception {
        // Try to start a WebappActivity.  Fail because the Intent is insecure.
        fireWebappIntent(WEBAPP_1_ID, WEBAPP_1_URL, WEBAPP_1_TITLE, WEBAPP_ICON, false);
        CriteriaHelper.pollUiThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                Activity lastActivity = ApplicationStatus.getLastTrackedFocusedActivity();
                return lastActivity instanceof ChromeTabbedActivity;
            }
        });
        ChromeActivity chromeActivity =
                (ChromeActivity) ApplicationStatus.getLastTrackedFocusedActivity();
        mTestRule.waitForFullLoad(chromeActivity, WEBAPP_1_TITLE);

        // Firing a correct Intent should start a WebappActivity instance instead of the browser.
        fireWebappIntent(WEBAPP_2_ID, WEBAPP_2_URL, WEBAPP_2_TITLE, WEBAPP_ICON, true);
        CriteriaHelper.pollUiThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return isWebappActivityReady(ApplicationStatus.getLastTrackedFocusedActivity());
            }
        });
    }

    /**
     * Test that a WebappActivity uses WebappInfo set via WebappActivity#putWebappInfo() if
     * available instead of constructing the WebappInfo from the launch intent.
     */
    @Test
    @MediumTest
    @Feature({"Webapps"})
    public void testWebappInfoReuse() {
        Intent intent = createIntent(
                WebappActivityTestRule.WEBAPP_ID, WEBAPP_2_URL, WEBAPP_2_TITLE, WEBAPP_ICON, true);
        Intent newIntent = createIntent(
                WebappActivityTestRule.WEBAPP_ID, WEBAPP_1_URL, WEBAPP_1_TITLE, WEBAPP_ICON, true);
        WebappInfo webappInfo = WebappInfo.create(intent);
        WebappActivity.addWebappInfo(WebappActivityTestRule.WEBAPP_ID, webappInfo);

        WebappActivityTestRule mActivityTestRule = new WebappActivityTestRule();
        mActivityTestRule.startWebappActivity(newIntent);

        CriteriaHelper.pollUiThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return isWebappActivityReady(ApplicationStatus.getLastTrackedFocusedActivity());
            }
        });

        Activity lastActivity = ApplicationStatus.getLastTrackedFocusedActivity();
        WebappActivity lastWebappActivity = (WebappActivity) lastActivity;

        Assert.assertEquals(webappInfo, lastWebappActivity.getWebappInfo());
        Assert.assertTrue(lastWebappActivity.getWebappInfo().url().equals(WEBAPP_2_URL));
    }

    /** Test that on first launch {@link WebappDataStorage#hasBeenLaunched()} is set. */
    // Flaky even with RetryOnFailure: http://crbug.com/749375
    @DisabledTest
    @Test
    //    @MediumTest
    //    @Feature({"Webapps"})
    public void testSetsHasBeenLaunchedOnFirstLaunch() {
        WebappDataStorage storage = WebappRegistry.getInstance().getWebappDataStorage(WEBAPP_1_ID);
        Assert.assertFalse(storage.hasBeenLaunched());

        startWebappActivity(WEBAPP_1_ID, WEBAPP_1_URL, WEBAPP_1_TITLE, WEBAPP_ICON);

        // Use a longer timeout because the DeferredStartupHandler is called after the page has
        // finished loading.
        CriteriaHelper.pollUiThread(new Criteria("Deferred startup never completed") {
            @Override
            public boolean isSatisfied() {
                return DeferredStartupHandler.getInstance().isDeferredStartupCompleteForApp();
            }
        }, 5000L, CriteriaHelper.DEFAULT_POLLING_INTERVAL);

        Assert.assertTrue(storage.hasBeenLaunched());
    }

    /**
     * Starts a WebappActivity for the given data and waits for it to be initialized.  We can't use
     * ActivityUtils.waitForActivity() because of the way WebappActivity is instanced on pre-L
     * devices.
     */
    private WebappActivity startWebappActivity(String id, String url, String title, String icon) {
        fireWebappIntent(id, url, title, icon, true);
        CriteriaHelper.pollUiThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                Activity lastActivity = ApplicationStatus.getLastTrackedFocusedActivity();
                return isWebappActivityReady(lastActivity);
            }
        }, 10000, CriteriaHelper.DEFAULT_POLLING_INTERVAL);
        return (WebappActivity) ApplicationStatus.getLastTrackedFocusedActivity();
    }

    /** Returns true when the last Activity is a WebappActivity and is ready for testing .*/
    private boolean isWebappActivityReady(Activity lastActivity) {
        if (!(lastActivity instanceof WebappActivity)) return false;

        WebappActivity webappActivity = (WebappActivity) lastActivity;
        if (webappActivity.getActivityTab() == null) return false;

        View rootView = webappActivity.findViewById(android.R.id.content);
        if (!rootView.hasWindowFocus()) return false;

        return true;
    }
}
