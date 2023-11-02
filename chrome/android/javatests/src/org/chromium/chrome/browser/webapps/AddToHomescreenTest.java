// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps;

import android.content.Context;
import android.content.Intent;
import android.graphics.Bitmap;
import android.os.Build;
import android.os.Handler;
import android.text.TextUtils;

import androidx.test.filters.SmallTest;

import org.hamcrest.Matchers;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DisableIf;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Restriction;
import org.chromium.base.test.util.UrlUtils;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ShortcutHelper;
import org.chromium.chrome.browser.app.ChromeActivity;
import org.chromium.chrome.browser.banners.AppMenuVerbiage;
import org.chromium.chrome.browser.browserservices.intents.BitmapHelper;
import org.chromium.chrome.browser.browserservices.intents.WebappConstants;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.browser.TabLoadObserver;
import org.chromium.chrome.test.util.browser.TabTitleObserver;
import org.chromium.chrome.test.util.browser.webapps.WebappTestPage;
import org.chromium.components.webapps.AddToHomescreenCoordinator;
import org.chromium.components.webapps.AddToHomescreenDialogView;
import org.chromium.components.webapps.AddToHomescreenViewDelegate;
import org.chromium.components.webapps.AppBannerManager;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.content_public.common.ContentSwitches;
import org.chromium.net.test.EmbeddedTestServerRule;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modaldialog.ModalDialogManager;

/**
 * Tests org.chromium.chrome.browser.webapps.addtohomescreen.AddToHomescreenManager and its C++
 * counterpart.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class AddToHomescreenTest {
    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    @Rule
    public EmbeddedTestServerRule mTestServerRule = new EmbeddedTestServerRule();

    private static final String WEBAPP_ACTION_NAME = "WEBAPP_ACTION";

    private static final String WEBAPP_TITLE = "Webapp shortcut";
    private static final String WEBAPP_HTML = UrlUtils.encodeHtmlDataUri("<html><head>"
            + "<meta name=\"mobile-web-app-capable\" content=\"yes\" />"
            + "<title>" + WEBAPP_TITLE + "</title>"
            + "</head><body>Webapp capable</body></html>");
    private static final String EDITED_WEBAPP_TITLE = "Webapp shortcut edited";

    private static final String SECOND_WEBAPP_TITLE = "Webapp shortcut #2";
    private static final String SECOND_WEBAPP_HTML = UrlUtils.encodeHtmlDataUri("<html><head>"
            + "<meta name=\"mobile-web-app-capable\" content=\"yes\" />"
            + "<title>" + SECOND_WEBAPP_TITLE + "</title>"
            + "</head><body>Webapp capable again</body></html>");

    private static final String NORMAL_TITLE = "Plain shortcut";
    private static final String NORMAL_HTML = UrlUtils.encodeHtmlDataUri("<html>"
            + "<head><title>" + NORMAL_TITLE + "</title></head>"
            + "<body>Not Webapp capable</body></html>");

    private static final String META_APP_NAME_PAGE_TITLE = "Not the right title";
    private static final String META_APP_NAME_TITLE = "Web application-name";
    private static final String META_APP_NAME_HTML = UrlUtils.encodeHtmlDataUri("<html><head>"
            + "<meta name=\"mobile-web-app-capable\" content=\"yes\" />"
            + "<meta name=\"application-name\" content=\"" + META_APP_NAME_TITLE + "\">"
            + "<title>" + META_APP_NAME_PAGE_TITLE + "</title>"
            + "</head><body>Webapp capable</body></html>");

    private static final String NON_MASKABLE_MANIFEST_TEST_PAGE_PATH =
            "/chrome/test/data/banners/manifest_test_page.html?manifest=manifest_empty_name_short_name.json";
    private static final String MASKABLE_MANIFEST_TEST_PAGE_PATH =
            "/chrome/test/data/banners/manifest_test_page.html?manifest=manifest_empty_name_short_name_maskable.json";
    private static final String MANIFEST_TEST_PAGE_TITLE = "Web app banner test page";

    private static final String INSTALL_PATH_HISTOGRAM_NAME = "WebApk.Install.PathToInstall";

    private static class TestShortcutHelperDelegate extends ShortcutHelper.Delegate {
        public String mRequestedShortcutTitle;
        public Intent mRequestedShortcutIntent;
        public boolean mRequestedShortcutAdaptable;

        @Override
        public void addShortcutToHomescreen(String id, String title, Bitmap icon,
                boolean iconAdaptable, Intent shortcutIntent) {
            mRequestedShortcutTitle = title;
            mRequestedShortcutIntent = shortcutIntent;
            mRequestedShortcutAdaptable = iconAdaptable;
        }

        @Override
        public String getFullscreenAction() {
            return WEBAPP_ACTION_NAME;
        }

        public void clearRequestedShortcutData() {
            mRequestedShortcutTitle = null;
            mRequestedShortcutIntent = null;
            mRequestedShortcutAdaptable = false;
        }
    }

    private static class TestDataStorageFactory extends WebappDataStorage.Factory {
        public String mSplashImage;

        @Override
        public WebappDataStorage create(final String webappId) {
            return new WebappDataStorageWrapper(webappId);
        }

        private class WebappDataStorageWrapper extends WebappDataStorage {
            public WebappDataStorageWrapper(String webappId) {
                super(webappId);
            }

            @Override
            public void updateSplashScreenImage(String splashScreenImage) {
                Assert.assertNull(mSplashImage);
                mSplashImage = splashScreenImage;
            }
        }
    }

    /**
     * Test TestAddToHomescreenCoordinator subclass which mocks showing the add-to-homescreen view
     * and adds the shortcut to the home screen once it is ready.
     */
    private static class TestAddToHomescreenCoordinator extends AddToHomescreenCoordinator {
        private String mTitle;

        TestAddToHomescreenCoordinator(WebContents webContents, Context context,
                WindowAndroid windowAndroid, ModalDialogManager modalDialogManager, String title) {
            super(webContents, context, windowAndroid, modalDialogManager);
            mTitle = title;
        }

        @Override
        protected AddToHomescreenDialogView initView(
                AppBannerManager.InstallStringPair installStrings,
                AddToHomescreenViewDelegate delegate) {
            return new AddToHomescreenDialogView(getContextForTests(),
                    getModalDialogManagerForTests(), installStrings, delegate) {
                @Override
                protected void setTitle(String title) {
                    if (TextUtils.isEmpty(mTitle)) {
                        mTitle = title;
                    }
                }

                @Override
                protected void setCanSubmit(boolean canSubmit) {
                    new Handler().post(() -> mDelegate.onAddToHomescreen(mTitle));
                }
            };
        }
    }

    private ChromeActivity mActivity;
    private Tab mTab;
    private TestShortcutHelperDelegate mShortcutHelperDelegate;

    @Before
    public void setUp() throws Exception {
        mActivityTestRule.startMainActivityOnBlankPage();
        mShortcutHelperDelegate = new TestShortcutHelperDelegate();
        ShortcutHelper.setDelegateForTests(mShortcutHelperDelegate);
        mActivity = mActivityTestRule.getActivity();
        mTab = mActivity.getActivityTab();
    }

    @Test
    @SmallTest
    @Feature("{Webapp}")
    public void testAddWebappShortcuts() throws Exception {
        // Add a webapp shortcut and make sure the intent's parameters make sense.
        loadUrl(WEBAPP_HTML, WEBAPP_TITLE);
        addShortcutToTab(mTab, "", true);
        Assert.assertEquals(WEBAPP_TITLE, mShortcutHelperDelegate.mRequestedShortcutTitle);

        Intent launchIntent = mShortcutHelperDelegate.mRequestedShortcutIntent;
        Assert.assertEquals(WEBAPP_HTML, launchIntent.getStringExtra(WebappConstants.EXTRA_URL));
        Assert.assertEquals(WEBAPP_ACTION_NAME, launchIntent.getAction());
        Assert.assertEquals(mActivity.getPackageName(), launchIntent.getPackage());

        // Add a second shortcut and make sure it matches the second webapp's parameters.
        mShortcutHelperDelegate.clearRequestedShortcutData();
        loadUrl(SECOND_WEBAPP_HTML, SECOND_WEBAPP_TITLE);
        addShortcutToTab(mTab, "", true);
        Assert.assertEquals(SECOND_WEBAPP_TITLE, mShortcutHelperDelegate.mRequestedShortcutTitle);

        Intent newLaunchIntent = mShortcutHelperDelegate.mRequestedShortcutIntent;
        Assert.assertEquals(
                SECOND_WEBAPP_HTML, newLaunchIntent.getStringExtra(WebappConstants.EXTRA_URL));
        Assert.assertEquals(WEBAPP_ACTION_NAME, newLaunchIntent.getAction());
        Assert.assertEquals(mActivity.getPackageName(), newLaunchIntent.getPackage());

        Assert.assertEquals(
                0, RecordHistogram.getHistogramTotalCountForTesting(INSTALL_PATH_HISTOGRAM_NAME));
    }

    @Test
    @SmallTest
    @Feature("{Webapp}")
    @DisableIf.Build(sdk_is_less_than = Build.VERSION_CODES.O)
    public void testAddAdaptableShortcut() throws Exception {
        // Test the baseline of no adaptive icon.
        loadUrl(mTestServerRule.getServer().getURL(NON_MASKABLE_MANIFEST_TEST_PAGE_PATH),
                MANIFEST_TEST_PAGE_TITLE);
        addShortcutToTab(mTab, "", true);

        Assert.assertFalse(mShortcutHelperDelegate.mRequestedShortcutAdaptable);

        mShortcutHelperDelegate.clearRequestedShortcutData();

        // Test the adaptive icon.
        loadUrl(mTestServerRule.getServer().getURL(MASKABLE_MANIFEST_TEST_PAGE_PATH),
                MANIFEST_TEST_PAGE_TITLE);
        addShortcutToTab(mTab, "", true);

        Assert.assertTrue(mShortcutHelperDelegate.mRequestedShortcutAdaptable);

        Assert.assertEquals(
                0, RecordHistogram.getHistogramTotalCountForTesting(INSTALL_PATH_HISTOGRAM_NAME));
    }

    @Test
    @SmallTest
    @Feature("{Webapp}")
    public void testAddBookmarkShortcut() throws Exception {
        loadUrl(NORMAL_HTML, NORMAL_TITLE);
        addShortcutToTab(mTab, "", true);

        // Make sure the intent's parameters make sense.
        Assert.assertEquals(NORMAL_TITLE, mShortcutHelperDelegate.mRequestedShortcutTitle);

        Intent launchIntent = mShortcutHelperDelegate.mRequestedShortcutIntent;
        Assert.assertEquals(mActivity.getPackageName(), launchIntent.getPackage());
        Assert.assertEquals(Intent.ACTION_VIEW, launchIntent.getAction());
        Assert.assertEquals(NORMAL_HTML, launchIntent.getDataString());

        Assert.assertEquals(
                0, RecordHistogram.getHistogramTotalCountForTesting(INSTALL_PATH_HISTOGRAM_NAME));
    }

    @Test
    @SmallTest
    @Feature("{Webapp}")
    public void testAddWebappShortcutsWithoutTitleEdit() throws Exception {
        // Add a webapp shortcut using the page's title.
        loadUrl(WEBAPP_HTML, WEBAPP_TITLE);
        addShortcutToTab(mTab, "", true);
        Assert.assertEquals(WEBAPP_TITLE, mShortcutHelperDelegate.mRequestedShortcutTitle);

        Assert.assertEquals(
                0, RecordHistogram.getHistogramTotalCountForTesting(INSTALL_PATH_HISTOGRAM_NAME));
    }

    @Test
    @SmallTest
    @Feature("{Webapp}")
    public void testAddWebappShortcutsWithTitleEdit() throws Exception {
        // Add a webapp shortcut with a custom title.
        loadUrl(WEBAPP_HTML, WEBAPP_TITLE);
        addShortcutToTab(mTab, EDITED_WEBAPP_TITLE, true);
        Assert.assertEquals(EDITED_WEBAPP_TITLE, mShortcutHelperDelegate.mRequestedShortcutTitle);

        Assert.assertEquals(
                0, RecordHistogram.getHistogramTotalCountForTesting(INSTALL_PATH_HISTOGRAM_NAME));
    }

    @Test
    @SmallTest
    @Feature("{Webapp}")
    public void testAddWebappShortcutsWithApplicationName() throws Exception {
        loadUrl(META_APP_NAME_HTML, META_APP_NAME_PAGE_TITLE);
        addShortcutToTab(mTab, "", true);
        Assert.assertEquals(META_APP_NAME_TITLE, mShortcutHelperDelegate.mRequestedShortcutTitle);

        Assert.assertEquals(
                0, RecordHistogram.getHistogramTotalCountForTesting(INSTALL_PATH_HISTOGRAM_NAME));
    }

    @Test
    @SmallTest
    @Feature("{Webapp}")
    @Restriction(Restriction.RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    @CommandLineFlags.Add(ContentSwitches.DISABLE_POPUP_BLOCKING)
    @DisabledTest(message = "https://crbug.com/1269022")
    public void testAddWebappShortcutWithEmptyPage() {
        Tab spawnedPopup = spawnPopupInBackground("");
        addShortcutToTab(spawnedPopup, "", true /* expectAdded */);

        Assert.assertEquals(
                0, RecordHistogram.getHistogramTotalCountForTesting(INSTALL_PATH_HISTOGRAM_NAME));
    }

    @Test
    @SmallTest
    @Feature("{Webapp}")
    @CommandLineFlags.Add({ContentSwitches.DISABLE_POPUP_BLOCKING,
            // TODO(yfriedman): Force WebApks off as this tests old A2HS behaviour.
            "disable-field-trial-config"})
    public void
    testAddWebappShortcutSplashScreenIcon() throws Exception {
        // Sets the overridden factory to observe splash screen update.
        final TestDataStorageFactory dataStorageFactory = new TestDataStorageFactory();
        WebappDataStorage.setFactoryForTests(dataStorageFactory);

        loadUrl(WebappTestPage.getServiceWorkerUrl(mTestServerRule.getServer()),
                WebappTestPage.PAGE_TITLE);
        addShortcutToTab(mTab, "", true);

        // Make sure that the splash screen image was downloaded.
        CriteriaHelper.pollUiThread(() -> {
            Criteria.checkThat(dataStorageFactory.mSplashImage, Matchers.notNullValue());
        });

        Assert.assertTrue(ShortcutHelper.sSplashImageMap.isEmpty());

        // Test that bitmap sizes match expectations.
        int idealSize = mActivity.getResources().getDimensionPixelSize(
                R.dimen.webapp_splash_image_size_ideal);
        Bitmap splashImage = BitmapHelper.decodeBitmapFromString(dataStorageFactory.mSplashImage);
        Assert.assertEquals(idealSize, splashImage.getWidth());
        Assert.assertEquals(idealSize, splashImage.getHeight());

        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        INSTALL_PATH_HISTOGRAM_NAME, /* kAppMenuInstall= */ 2));
    }

    /**
     * Tests that the appinstalled event is fired when an app is installed.
     */
    @Test
    @SmallTest
    @Feature("{Webapp}")
    public void testAddWebappShortcutAppInstalledEvent() throws Exception {
        loadUrl(WebappTestPage.getServiceWorkerUrlWithAction(
                        mTestServerRule.getServer(), "verify_appinstalled"),
                WebappTestPage.PAGE_TITLE);
        addShortcutToTab(mTab, "", true);

        // Wait for the tab title to change. This will happen (due to the JavaScript that runs
        // in the page) once the appinstalled event has been fired twice: once to test
        // addEventListener('appinstalled'), once to test onappinstalled attribute.
        new TabTitleObserver(mTab, "Got appinstalled: listener, attr").waitForTitleUpdate(3);

        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        INSTALL_PATH_HISTOGRAM_NAME, /* kAppMenuInstall= */ 2));
    }

    private void loadUrl(String url, String expectedPageTitle) throws Exception {
        new TabLoadObserver(mTab, expectedPageTitle, null).fullyLoadUrl(url);
    }

    private void addShortcutToTab(Tab tab, String title, boolean expectAdded) {
        // Add the shortcut.
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            boolean started =
                    new TestAddToHomescreenCoordinator(tab.getWebContents(), mActivity,
                            mActivity.getWindowAndroid(), mActivity.getModalDialogManager(), title)
                            .showForAppMenu(AppMenuVerbiage.APP_MENU_OPTION_ADD_TO_HOMESCREEN);
            Assert.assertEquals(expectAdded, started);
        });

        // Make sure that the shortcut was added.
        if (expectAdded) {
            CriteriaHelper.pollUiThread(() -> {
                Criteria.checkThat(
                        mShortcutHelperDelegate.mRequestedShortcutIntent, Matchers.notNullValue());
            });
        }
    }

    /**
     * Spawns popup via window.open() at {@link url}.
     */
    private Tab spawnPopupInBackground(final String url) {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mTab.getWebContents().evaluateJavaScriptForTests("(function() {"
                            + "window.open('" + url + "');"
                            + "})()",
                    null);
        });

        CriteriaHelper.pollUiThread(() -> {
            Criteria.checkThat(mActivityTestRule.getActivity()
                                       .getTabModelSelector()
                                       .getModel(false)
                                       .getCount(),
                    Matchers.is(2));
        });

        TabModel tabModel = mActivityTestRule.getActivity().getTabModelSelector().getModel(false);
        Assert.assertEquals(0, tabModel.indexOf(mTab));
        return mActivityTestRule.getActivity().getTabModelSelector().getModel(false).getTabAt(1);
    }
}
