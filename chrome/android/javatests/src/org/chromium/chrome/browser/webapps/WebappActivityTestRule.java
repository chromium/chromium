// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps;

import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;

import android.content.Intent;
import android.net.Uri;
import android.view.View;
import android.view.ViewGroup;

import androidx.browser.customtabs.TrustedWebUtils;
import androidx.test.core.app.ApplicationProvider;
import androidx.test.platform.app.InstrumentationRegistry;

import org.hamcrest.Matchers;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.UrlUtils;
import org.chromium.cc.input.BrowserControlsState;
import org.chromium.chrome.browser.app.ChromeActivity;
import org.chromium.chrome.browser.browserservices.intents.WebappConstants;
import org.chromium.chrome.browser.browserservices.ui.splashscreen.SplashController;
import org.chromium.chrome.browser.customtabs.CustomTabsIntentTestUtils;
import org.chromium.chrome.browser.tab.TabBrowserControlsConstraintsHelper;
import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.content_public.browser.test.util.DOMUtils;
import org.chromium.content_public.browser.test.util.JavaScriptUtils;

/** Custom {@link ChromeActivityTestRule} for tests using {@link WebappActivity}. */
public class WebappActivityTestRule extends ChromeActivityTestRule<WebappActivity> {
    public static final String WEBAPP_ID = "webapp_id";
    public static final String WEBAPP_NAME = "webapp name";
    public static final String WEBAPP_SHORT_NAME = "webapp short name";

    private static final long STARTUP_TIMEOUT = 15000L;

    // Empty 192x192 image generated with:
    // ShortcutHelper.encodeBitmapAsString(Bitmap.createBitmap(192, 192, Bitmap.Config.ARGB_4444));
    public static final String TEST_ICON =
            "iVBORw0KGgoAAAANSUhEUgAAAMAAAADACAYAAABS3GwHAAAABHNCSVQICAgIfAhkiAAAAKZJREFU"
                    + "eJztwTEBAAAAwqD1T20JT6AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
                    + "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
                    + "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAD4GQN4AAe3mX6IA"
                    + "AAAASUVORK5CYII=";

    // Empty 512x512 image generated with:
    // ShortcutHelper.encodeBitmapAsString(Bitmap.createBitmap(512, 512, Bitmap.Config.ARGB_4444));
    public static final String TEST_SPLASH_ICON =
            "iVBORw0KGgoAAAANSUhEUgAAAgAAAAIACAYAAAD0eNT6AAAABHNCSVQICAgIfAhkiAAABA9JREFU"
                    + "eJztwTEBAAAAwqD1T20Hb6AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
                    + "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
                    + "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
                    + "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
                    + "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
                    + "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
                    + "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
                    + "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
                    + "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
                    + "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
                    + "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
                    + "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
                    + "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
                    + "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
                    + "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
                    + "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
                    + "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
                    + "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
                    + "AAAAAAAAAOA3AvAAAdln8YgAAAAASUVORK5CYII=";

    public WebappActivityTestRule() {
        super(WebappActivity.class);
    }

    /**
     * Creates the Intent that starts the WebAppActivity. This is meant to be overriden by other
     * tests in order for them to pass some specific values, but it defaults to a web app that just
     * loads about:blank to avoid a network load. This results in the URL bar showing because {@link
     * UrlUtils} cannot parse this type of URL.
     */
    public Intent createIntent() {
        Intent intent =
                new Intent(ApplicationProvider.getApplicationContext(), WebappActivity.class);
        intent.setAction(Intent.ACTION_VIEW);
        intent.setData(Uri.parse(WebappActivity.WEBAPP_SCHEME + "://" + WEBAPP_ID));
        intent.putExtra(WebappConstants.EXTRA_ID, WEBAPP_ID);
        intent.putExtra(WebappConstants.EXTRA_URL, "about:blank");
        intent.putExtra(WebappConstants.EXTRA_NAME, WEBAPP_NAME);
        intent.putExtra(WebappConstants.EXTRA_SHORT_NAME, WEBAPP_SHORT_NAME);
        return intent;
    }

    /** Adds a mock Custom Tab session token to the intent. */
    public void addTwaExtrasToIntent(Intent intent) {
        Intent cctIntent =
                CustomTabsIntentTestUtils.createMinimalCustomTabIntent(
                        ApplicationProvider.getApplicationContext(), "about:blank");
        intent.putExtras(cctIntent.getExtras());
        intent.putExtra(TrustedWebUtils.EXTRA_LAUNCH_AS_TRUSTED_WEB_ACTIVITY, true);
    }

    @Override
    protected void before() throws Throwable {
        super.before();
        // We run the WebappRegistry calls on the UI thread to prevent
        // ConcurrentModificationExceptions caused by multiple threads iterating and
        // modifying its hashmap at the same time.
        final TestFetchStorageCallback callback = new TestFetchStorageCallback();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    // Register the webapp so when the data storage is opened, the
                    // test doesn't crash.
                    WebappRegistry.refreshSharedPrefsForTesting();
                    WebappRegistry.getInstance().register(WEBAPP_ID, callback);
                });

        // Running this on the UI thread causes issues, so can't group everything
        // into one runnable.
        callback.waitForCallback(0);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    callback.getStorage()
                            .updateFromWebappIntentDataProvider(
                                    WebappIntentDataProviderFactory.create(createIntent()));
                });
    }

    @Override
    protected void after() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    WebappRegistry.getInstance().clearForTesting();
                });
        super.after();
    }

    /** Starts up the WebappActivity and sets up the test observer. */
    public final void startWebappActivity() {
        startWebappActivity(createIntent());
    }

    /** Starts up the WebappActivity with a specific Intent and sets up the test observer. */
    public final void startWebappActivity(Intent intent) {
        String startUrl = intent.getStringExtra(WebappConstants.EXTRA_URL);

        launchActivity(intent);

        WebappActivity webappActivity = getActivity();
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();

        CriteriaHelper.pollInstrumentationThread(
                () -> {
                    Criteria.checkThat(webappActivity.getActivityTab(), Matchers.notNullValue());
                },
                STARTUP_TIMEOUT,
                CriteriaHelper.DEFAULT_POLLING_INTERVAL);

        ChromeTabUtils.waitForTabPageLoaded(webappActivity.getActivityTab(), startUrl);
        waitUntilSplashscreenHides();
    }

    public static void assertToolbarShownMaybeHideable(ChromeActivity activity) {
        @BrowserControlsState int state = getToolbarShowState(activity);
        assertTrue(state == BrowserControlsState.SHOWN || state == BrowserControlsState.BOTH);
    }

    public static @BrowserControlsState int getToolbarShowState(ChromeActivity activity) {
        return ThreadUtils.runOnUiThreadBlocking(
                () ->
                        TabBrowserControlsConstraintsHelper.getConstraints(
                                activity.getActivityTab()));
    }

    /**
     * Executing window.open() through a click on a link, as it needs user gesture to avoid Chrome
     * blocking it as a popup.
     */
    public static void jsWindowOpen(ChromeActivity activity, String url) throws Exception {
        String injectedHtml =
                String.format(
                        "var aTag = document.createElement('testId');"
                                + "aTag.id = 'testId';"
                                + "aTag.innerHTML = 'Click Me!';"
                                + "aTag.onclick = function() {"
                                + "  window.open('%s');"
                                + "  return false;"
                                + "};"
                                + "document.body.insertAdjacentElement('afterbegin', aTag);",
                        url);
        JavaScriptUtils.executeJavaScriptAndWaitForResult(
                activity.getActivityTab().getWebContents(), injectedHtml);
        DOMUtils.clickNode(activity.getActivityTab().getWebContents(), "testId");
    }

    /**
     * Starts up the WebappActivity and sets up the test observer. Wait till Splashscreen full
     * loaded.
     */
    public final ViewGroup startWebappActivityAndWaitForSplashScreen() {
        return startWebappActivityAndWaitForSplashScreen(createIntent());
    }

    /**
     * Starts up the WebappActivity and sets up the test observer. Wait till Splashscreen full
     * loaded. Intent url is modified to one that takes more time to load.
     */
    public final ViewGroup startWebappActivityAndWaitForSplashScreen(Intent intent) {
        // Reset the url to one that takes more time to load.
        // This is to make sure splash screen won't disappear during test.
        intent.putExtra(WebappConstants.EXTRA_URL, getTestServer().getURL("/slow?2"));
        launchActivity(intent);
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();
        CriteriaHelper.pollInstrumentationThread(
                () -> {
                    // we are waiting for WebappActivity#getActivityTab() to be non-null because we
                    // want to ensure that native has been loaded.
                    // We also wait till the splash screen has finished initializing.
                    Criteria.checkThat(getActivity().getActivityTab(), Matchers.notNullValue());

                    View splashScreen =
                            getSplashController(getActivity()).getSplashScreenForTests();
                    Criteria.checkThat(splashScreen, Matchers.notNullValue());

                    if (!(splashScreen instanceof ViewGroup)) return;
                    Criteria.checkThat(
                            ((ViewGroup) splashScreen).getChildCount(), Matchers.greaterThan(0));
                },
                STARTUP_TIMEOUT,
                CriteriaHelper.DEFAULT_POLLING_INTERVAL);

        InstrumentationRegistry.getInstrumentation().waitForIdleSync();
        View splashScreen = getSplashController(getActivity()).getSplashScreenForTests();
        assertNotNull("No splash screen available.", splashScreen);

        // TODO(pkotwicz): Change return type in order to accommodate new-style WebAPKs.
        // (crbug.com/958288)
        return (splashScreen instanceof ViewGroup) ? (ViewGroup) splashScreen : null;
    }

    /** Waits for the splash screen to be hidden. */
    public void waitUntilSplashscreenHides() {
        waitUntilSplashHides(getActivity());
    }

    public static void waitUntilSplashHides(WebappActivity activity) {
        CriteriaHelper.pollInstrumentationThread(
                () -> {
                    return getSplashController(activity).wasSplashScreenHiddenForTests();
                },
                STARTUP_TIMEOUT,
                CriteriaHelper.DEFAULT_POLLING_INTERVAL);
    }

    public boolean isSplashScreenVisible() {
        return getSplashController(getActivity()).getSplashScreenForTests() != null;
    }

    public static SplashController getSplashController(WebappActivity activity) {
        return activity.getComponent().resolveSplashController();
    }
}
