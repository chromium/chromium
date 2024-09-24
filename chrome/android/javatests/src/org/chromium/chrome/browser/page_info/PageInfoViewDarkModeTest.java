// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.page_info;

import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withId;

import static org.hamcrest.CoreMatchers.allOf;
import static org.junit.Assert.assertNotNull;

import static org.chromium.ui.test.util.ViewUtils.onViewWaiting;

import android.view.View;

import androidx.test.filters.MediumTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DisableIf;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.chrome.browser.app.ChromeActivity;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.night_mode.ChromeNightModeTestUtils;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.R;
import org.chromium.components.page_info.PageInfoController;
import org.chromium.components.page_info.PageInfoController.OpenedFromSource;
import org.chromium.content_public.common.ContentSwitches;
import org.chromium.net.test.EmbeddedTestServerRule;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.test.util.RenderTestRule;

import java.io.IOException;

/**
 * Tests for PageInfoView. Uses pixel tests to ensure the UI handles different configurations
 * correctly. These tests are not batched because theme changes don't seem to work with batched
 * tests even with RequiresRestart as it results in the current {@link Tab} in the {@link
 * ChromeTabbedActivityTestRule} to be null.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({
    ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
    ChromeSwitches.DISABLE_STARTUP_PROMOS,
    ContentSwitches.HOST_RESOLVER_RULES + "=MAP * 127.0.0.1"
})
public class PageInfoViewDarkModeTest {
    private static final String sSimpleHtml = "/chrome/test/data/android/simple.html";

    @Rule
    public final ChromeTabbedActivityTestRule mActivityTestRule =
            new ChromeTabbedActivityTestRule();

    @Rule public EmbeddedTestServerRule mTestServerRule = new EmbeddedTestServerRule();

    @Rule
    public RenderTestRule mRenderTestRule =
            RenderTestRule.Builder.withPublicCorpus()
                    .setRevision(5)
                    .setBugComponent(RenderTestRule.Component.UI_BROWSER_BUBBLES_PAGE_INFO)
                    .build();

    private void loadUrlAndOpenPageInfo(String url) {
        mActivityTestRule.loadUrl(url);
        openPageInfo();
    }

    private void openPageInfo() {
        ChromeActivity activity = mActivityTestRule.getActivity();
        Tab tab = activity.getActivityTab();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    new ChromePageInfo(
                                    activity.getModalDialogManagerSupplier(),
                                    null,
                                    OpenedFromSource.TOOLBAR,
                                    null,
                                    null,
                                    null)
                            .show(tab, ChromePageInfoHighlight.noHighlight());
                });
        onViewWaiting(
                allOf(withId(R.id.page_info_url_wrapper), isDisplayed()),
                true // Put Focus on dialog to fix flakiness in api 29+ with espresso 3.2.
                );
    }

    private View getPageInfoView() {
        PageInfoController controller = PageInfoController.getLastPageInfoControllerForTesting();
        assertNotNull(controller);
        View view = controller.getPageInfoViewForTesting();
        assertNotNull(view);
        return view;
    }

    @Before
    public void setUp() throws InterruptedException {
        // Choose a fixed, "random" port to create stable screenshots.
        mTestServerRule.setServerPort(424242);
        mTestServerRule.setServerUsesHttps(true);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    ChromeNightModeTestUtils.setUpNightModeForChromeActivity(
                            /* nightModeEnabled= */ true);
                });
        mActivityTestRule.startMainActivityOnBlankPage();
    }

    @After
    public void tearDown() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    ChromeNightModeTestUtils.setUpNightModeForChromeActivity(
                            /* nightModeEnabled= */ false);
                });
    }

    /** Tests the PageInfo UI on a secure website in dark mode. */
    @Test
    @MediumTest
    @Feature({"RenderTest"})
    @DisableFeatures(ChromeFeatureList.TRACKING_PROTECTION_3PCD)
    public void testShowOnSecureWebsiteDark() throws IOException {
        loadUrlAndOpenPageInfo(mTestServerRule.getServer().getURL(sSimpleHtml));
        mRenderTestRule.render(getPageInfoView(), "PageInfo_SecureWebsiteDark");
    }

    /** Tests PageInfo on internal page. */
    @Test
    @MediumTest
    @DisableIf.Device(DeviceFormFactor.TABLET) // https://crbug.com/338978357
    @Feature({"RenderTest"})
    public void testChromePage() throws IOException {
        loadUrlAndOpenPageInfo("chrome://version/");
        mRenderTestRule.render(getPageInfoView(), "PageInfo_InternalSiteDark");
    }
}
