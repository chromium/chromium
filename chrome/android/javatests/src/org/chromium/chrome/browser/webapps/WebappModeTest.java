// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps;

import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.view.View;

import androidx.test.core.app.ApplicationProvider;
import androidx.test.filters.MediumTest;
import androidx.test.platform.app.InstrumentationRegistry;

import org.hamcrest.Matchers;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ApplicationStatus;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.UrlUtils;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.ShortcutHelper;
import org.chromium.chrome.browser.browserservices.intents.BrowserServicesIntentDataProvider;
import org.chromium.chrome.browser.browserservices.intents.WebappConstants;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.ChromeApplicationTestUtils;
import org.chromium.chrome.test.util.browser.webapps.WebappTestHelper;

/**
 * Tests that WebappActivities are launched correctly.
 *
 * <p>This test seems a little wonky because WebappActivities launched differently, depending on
 * what OS the user is on. Pre-L, WebappActivities were manually instanced and assigned by the
 * WebappManager. On L and above, WebappActivities are automatically instanced by Android and the
 * FLAG_ACTIVITY_NEW_DOCUMENT mechanism. Moreover, we don't have access to the task list pre-L so we
 * have to assume that any non-running WebappActivities are not listed in Android's Overview.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class WebappModeTest {
    private static final String WEBAPP_1_ID = "webapp_id_1";
    private static final String WEBAPP_1_URL =
            UrlUtils.encodeHtmlDataUri(
                    "<html><head><title>Web app #1</title><meta name='viewport'"
                            + " content='width=device-width initial-scale=0.5,"
                            + " maximum-scale=0.5'></head><body bgcolor='#011684'>Webapp"
                            + " 1</body></html>");
    private static final String WEBAPP_1_TITLE = "Web app #1";

    private static final String WEBAPP_2_ID = "webapp_id_2";
    private static final String WEBAPP_2_URL =
            UrlUtils.encodeHtmlDataUri("<html><body bgcolor='#840116'>Webapp 2</body></html>");
    private static final String WEBAPP_2_TITLE = "Web app #2";

    private static final String WEBAPP_ICON =
            "iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAIAAACQd1PeAAAACXB"
                + "IWXMAAAsTAAALEwEAmpwYAAAAB3RJTUUH3wQIFB4cxOfiSQAAABl0RVh0Q29tbWVudABDcmVhdGVkIHdpdG"
                + "ggR0lNUFeBDhcAAAAMSURBVAjXY2AUawEAALcAnI/TkI8AAAAASUVORK5CYII=";

    private Intent createIntent(String id, String url, String title, String icon, boolean addMac) {
        Intent intent = WebappTestHelper.createMinimalWebappIntent(id, url);
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        intent.setPackage(ApplicationProvider.getApplicationContext().getPackageName());
        intent.setAction(WebappLauncherActivity.ACTION_START_WEBAPP);
        if (addMac) {
            // Needed for security reasons.  If the MAC is excluded, the URL of the webapp is opened
            // in a browser window, instead.
            String mac = ShortcutHelper.getEncodedMac(url);
            intent.putExtra(WebappConstants.EXTRA_MAC, mac);
        }

        intent.putExtra(WebappConstants.EXTRA_ICON, icon);
        intent.putExtra(WebappConstants.EXTRA_NAME, title);
        return intent;
    }

    private void fireWebappIntent(
            String id, String url, String title, String icon, boolean addMac) {
        Intent intent = createIntent(id, url, title, icon, addMac);

        ApplicationProvider.getApplicationContext().startActivity(intent);
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();
        ChromeApplicationTestUtils.waitUntilChromeInForeground();
    }

    @Before
    public void setUp() {
        ThreadUtils.runOnUiThreadBlocking(this::setUpOnUiThread);
    }

    private void setUpOnUiThread() {
        WebappRegistry.refreshSharedPrefsForTesting();

        // Register the webapps so when the data storage is opened, the test doesn't
        // crash.
        // There is no race condition with the retrieval as AsyncTasks are run
        // sequentially on the background thread.
        WebappRegistry.getInstance()
                .register(
                        WEBAPP_1_ID,
                        new WebappRegistry.FetchWebappDataStorageCallback() {
                            @Override
                            public void onWebappDataStorageRetrieved(WebappDataStorage storage) {
                                BrowserServicesIntentDataProvider intentDataProvider =
                                        WebappIntentDataProviderFactory.create(
                                                createIntent(
                                                        WEBAPP_1_ID,
                                                        WEBAPP_1_URL,
                                                        WEBAPP_1_TITLE,
                                                        WEBAPP_ICON,
                                                        true));
                                storage.updateFromWebappIntentDataProvider(intentDataProvider);
                            }
                        });
        WebappRegistry.getInstance()
                .register(
                        WEBAPP_2_ID,
                        new WebappRegistry.FetchWebappDataStorageCallback() {
                            @Override
                            public void onWebappDataStorageRetrieved(WebappDataStorage storage) {
                                BrowserServicesIntentDataProvider intentDataProvider =
                                        WebappIntentDataProviderFactory.create(
                                                createIntent(
                                                        WEBAPP_1_ID,
                                                        WEBAPP_1_URL,
                                                        WEBAPP_1_TITLE,
                                                        WEBAPP_ICON,
                                                        true));
                                storage.updateFromWebappIntentDataProvider(intentDataProvider);
                            }
                        });
    }

    /** Tests that WebappActivities are started properly. */
    @Test
    @MediumTest
    @Feature({"Webapps"})
    public void testWebappLaunches() {
        final WebappActivity firstActivity =
                startWebappActivity(WEBAPP_1_ID, WEBAPP_1_URL, WEBAPP_1_TITLE, WEBAPP_ICON);
        final int firstTabId = firstActivity.getActivityTab().getId();

        // Firing a different Intent should start a new WebappActivity instance.
        fireWebappIntent(WEBAPP_2_ID, WEBAPP_2_URL, WEBAPP_2_TITLE, WEBAPP_ICON, true);
        CriteriaHelper.pollUiThread(
                () -> {
                    Activity lastActivity = ApplicationStatus.getLastTrackedFocusedActivity();
                    Criteria.checkThat(isWebappActivityReady(lastActivity), Matchers.is(true));

                    WebappActivity lastWebappActivity = (WebappActivity) lastActivity;
                    Criteria.checkThat(
                            lastWebappActivity.getActivityTab().getId(), Matchers.not(firstTabId));
                });

        // Firing the first Intent should bring back the first WebappActivity instance, or at least
        // a WebappActivity with the same tab if the other one was killed by Android mid-test.
        fireWebappIntent(WEBAPP_1_ID, WEBAPP_1_URL, WEBAPP_1_TITLE, WEBAPP_ICON, true);
        CriteriaHelper.pollUiThread(
                () -> {
                    Activity lastActivity = ApplicationStatus.getLastTrackedFocusedActivity();
                    Criteria.checkThat(isWebappActivityReady(lastActivity), Matchers.is(true));

                    WebappActivity lastWebappActivity = (WebappActivity) lastActivity;
                    Criteria.checkThat(
                            lastWebappActivity.getActivityTab().getId(), Matchers.is(firstTabId));
                });
    }

    /** Tests that the WebappActivity gets the next available Tab ID instead of 0. */
    @Test
    @MediumTest
    @Feature({"Webapps"})
    public void testWebappTabIdsProperlyAssigned() {
        ChromeSharedPreferences.getInstance()
                .writeInt(ChromePreferenceKeys.TAB_ID_MANAGER_NEXT_ID, 11684);

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
        final Context context = ApplicationProvider.getApplicationContext();
        ChromeApplicationTestUtils.fireHomeScreenIntent(context);
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();

        // Bring the WebappActivity back via an Intent.
        Intent intent =
                IntentHandler.createTrustedBringTabToFrontIntent(
                        webappTabId, IntentHandler.BringToFrontSource.NOTIFICATION);
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        context.startActivity(intent);

        // When Chrome is back in the foreground, confirm that the correct Activity was restored.
        // Because of Android killing Activities willy-nilly, it may not be the same Activity, but
        // it should have the same Tab ID.
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();
        ChromeApplicationTestUtils.waitUntilChromeInForeground();
        CriteriaHelper.pollInstrumentationThread(
                () -> {
                    Activity lastActivity = ApplicationStatus.getLastTrackedFocusedActivity();
                    Criteria.checkThat(isWebappActivityReady(lastActivity), Matchers.is(true));

                    WebappActivity lastWebappActivity = (WebappActivity) lastActivity;
                    Criteria.checkThat(
                            lastWebappActivity.getActivityTab().getId(), Matchers.is(webappTabId));
                });
    }

    /**
     * Starts a WebappActivity for the given data and waits for it to be initialized. We can't use
     * ActivityTestUtils.waitForActivity() because of the way WebappActivity is instanced on pre-L
     * devices.
     */
    private WebappActivity startWebappActivity(String id, String url, String title, String icon) {
        fireWebappIntent(id, url, title, icon, true);
        CriteriaHelper.pollUiThread(
                () -> {
                    Activity lastActivity = ApplicationStatus.getLastTrackedFocusedActivity();
                    return isWebappActivityReady(lastActivity);
                },
                10000,
                CriteriaHelper.DEFAULT_POLLING_INTERVAL);
        return (WebappActivity) ApplicationStatus.getLastTrackedFocusedActivity();
    }

    /** Returns true when the last Activity is a WebappActivity and is ready for testing . */
    private boolean isWebappActivityReady(Activity lastActivity) {
        if (!(lastActivity instanceof WebappActivity)) return false;

        WebappActivity webappActivity = (WebappActivity) lastActivity;
        if (webappActivity.getActivityTab() == null) return false;

        View rootView = webappActivity.findViewById(android.R.id.content);
        if (!rootView.hasWindowFocus()) return false;

        return true;
    }
}
