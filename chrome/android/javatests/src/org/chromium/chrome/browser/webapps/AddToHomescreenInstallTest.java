// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps;

import android.content.Context;
import android.os.Handler;
import android.text.TextUtils;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.app.ChromeActivity;
import org.chromium.chrome.browser.banners.AppMenuVerbiage;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.browser.TabLoadObserver;
import org.chromium.components.webapps.AddToHomescreenCoordinator;
import org.chromium.components.webapps.AddToHomescreenDialogView;
import org.chromium.components.webapps.AddToHomescreenViewDelegate;
import org.chromium.components.webapps.AppBannerManager;
import org.chromium.components.webapps.AppType;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.net.test.EmbeddedTestServerRule;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.test.util.DeviceRestriction;

/**
 * Tests org.chromium.chrome.browser.webapps.addtohomescreen.AddToHomescreenManager and its C++
 * counterpart.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Restriction(DeviceRestriction.RESTRICTION_TYPE_NON_AUTO)
public class AddToHomescreenInstallTest {
    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    @Rule public EmbeddedTestServerRule mTestServerRule = new EmbeddedTestServerRule();

    private static final String MANIFEST_TEST_PAGE_PATH =
            "/chrome/test/data/banners/manifest_test_page.html";
    private static final String MANIFEST_TEST_PAGE_TITLE = "Web app banner test page";

    private static final String INSTALL_PATH_HISTOGRAM_NAME = "WebApk.Install.PathToInstall";

    /**
     * Test TestAddToHomescreenCoordinator subclass which mocks showing the add-to-homescreen view
     * and adds the shortcut to the home screen once it is ready.
     */
    private static class TestAddToHomescreenCoordinator extends AddToHomescreenCoordinator {
        private String mTitle;

        TestAddToHomescreenCoordinator(
                WebContents webContents,
                Context context,
                WindowAndroid windowAndroid,
                ModalDialogManager modalDialogManager,
                String title) {
            super(webContents, context, windowAndroid, modalDialogManager);
            mTitle = title;
        }

        @Override
        protected AddToHomescreenDialogView initView(
                AppBannerManager.InstallStringPair installStrings,
                AddToHomescreenViewDelegate delegate) {
            return new AddToHomescreenDialogView(
                    getContextForTests(),
                    getModalDialogManagerForTests(),
                    installStrings,
                    delegate) {
                @Override
                protected void setTitle(String title) {
                    if (TextUtils.isEmpty(mTitle)) {
                        mTitle = title;
                    }
                }

                @Override
                protected void setCanSubmit(boolean canSubmit) {
                    new Handler().post(() -> mDelegate.onAddToHomescreen(mTitle, AppType.WEBAPK));
                }
            };
        }
    }

    private ChromeActivity mActivity;
    private Tab mTab;
    private HistogramWatcher mInstallHistogramsWatcher;

    @Before
    public void setUp() throws Exception {
        mActivityTestRule.startMainActivityOnBlankPage();
        mActivity = mActivityTestRule.getActivity();
        mTab = mActivity.getActivityTab();
        mInstallHistogramsWatcher =
                HistogramWatcher.newSingleRecordWatcher("Webapp.Install.InstallEvent", 0);
    }

    private void loadUrl(String url, String expectedPageTitle) throws Exception {
        new TabLoadObserver(mTab, expectedPageTitle, null).fullyLoadUrl(url);
    }

    private void addToHomescreen(Tab tab, String title, boolean expectAdded) {
        // Add the shortcut.
        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    boolean started =
                            new TestAddToHomescreenCoordinator(
                                            tab.getWebContents(),
                                            mActivity,
                                            mActivity.getWindowAndroid(),
                                            mActivity.getModalDialogManager(),
                                            title)
                                    .showForAppMenu(AppMenuVerbiage.APP_MENU_OPTION_INSTALL);
                    Assert.assertEquals(expectAdded, started);
                });

        // Make sure that the shortcut was added.
        if (expectAdded) {
            mInstallHistogramsWatcher.pollInstrumentationThreadUntilSatisfied();
        }
    }

    @Test
    @SmallTest
    @Feature("{Webapp}")
    public void testInstallWebApk() throws Exception {
        HistogramWatcher histogram =
                HistogramWatcher.newSingleRecordWatcher(INSTALL_PATH_HISTOGRAM_NAME, 2);

        // Test the baseline of no adaptive icon.
        loadUrl(
                mTestServerRule.getServer().getURL(MANIFEST_TEST_PAGE_PATH),
                MANIFEST_TEST_PAGE_TITLE);
        addToHomescreen(mTab, "", true);

        histogram.assertExpected();
    }
}
