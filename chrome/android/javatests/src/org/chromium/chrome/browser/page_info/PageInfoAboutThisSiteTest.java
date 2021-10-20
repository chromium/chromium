// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.page_info;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.CoreMatchers.allOf;
import static org.hamcrest.CoreMatchers.containsString;
import static org.hamcrest.CoreMatchers.not;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.doReturn;

import static org.chromium.ui.test.util.ViewUtils.onViewWaiting;

import androidx.test.filters.MediumTest;

import org.junit.Before;
import org.junit.ClassRule;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.app.ChromeActivity;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.batch.BlankCTATabInitialStateRule;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.components.page_info.PageInfoController;
import org.chromium.components.page_info.proto.AboutThisSiteMetadataProto.SiteDescription;
import org.chromium.components.page_info.proto.AboutThisSiteMetadataProto.SiteInfo;
import org.chromium.content_public.browser.BrowserContextHandle;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.net.test.EmbeddedTestServerRule;
import org.chromium.ui.test.util.DisableAnimationsTestRule;
import org.chromium.url.GURL;

/**
 * Tests for PageInfoAboutThisSite.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@Features.EnableFeatures(ChromeFeatureList.PAGE_INFO_ABOUT_THIS_SITE)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Batch(Batch.PER_CLASS)
public class PageInfoAboutThisSiteTest {
    private static final String sSimpleHtml = "/chrome/test/data/android/simple.html";

    @ClassRule
    public static final ChromeTabbedActivityTestRule sActivityTestRule =
            new ChromeTabbedActivityTestRule();

    @ClassRule
    public static DisableAnimationsTestRule sDisableAnimationsTestRule =
            new DisableAnimationsTestRule();

    @Rule
    public final BlankCTATabInitialStateRule mInitialStateRule =
            new BlankCTATabInitialStateRule(sActivityTestRule, false);

    @Rule
    public EmbeddedTestServerRule mTestServerRule = new EmbeddedTestServerRule();

    @Rule
    public JniMocker mMocker = new JniMocker();

    @Mock
    private PageInfoAboutThisSiteController.Natives mMockAboutThisSiteJni;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mMocker.mock(PageInfoAboutThisSiteControllerJni.TEST_HOOKS, mMockAboutThisSiteJni);

        sActivityTestRule.loadUrl(mTestServerRule.getServer().getURL(sSimpleHtml));
    }

    private void openPageInfo() {
        ChromeActivity activity = sActivityTestRule.getActivity();
        Tab tab = activity.getActivityTab();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            new ChromePageInfo(activity.getModalDialogManagerSupplier(), null,
                    PageInfoController.OpenedFromSource.TOOLBAR, null)
                    .show(tab, PageInfoController.NO_HIGHLIGHTED_PERMISSION, false);
        });
        onViewWaiting(allOf(withId(org.chromium.chrome.R.id.page_info_url_wrapper), isDisplayed()));
    }

    @Test
    @MediumTest
    public void testStoreInfoRowWithData() {
        SiteDescription description =
                SiteDescription.newBuilder().setDescription("Some description").build();
        byte[] bytes = SiteInfo.newBuilder().setDescription(description).build().toByteArray();
        doReturn(bytes)
                .when(mMockAboutThisSiteJni)
                .getSiteInfo(any(BrowserContextHandle.class), any(GURL.class));
        openPageInfo();
        onView(withId(PageInfoAboutThisSiteController.ROW_ID)).check(matches(isDisplayed()));
        onView(withText(containsString("Some description"))).check(matches(isDisplayed()));
    }

    @Test
    @MediumTest
    public void testStoreInfoRowWithoutData() {
        doReturn(null)
                .when(mMockAboutThisSiteJni)
                .getSiteInfo(any(BrowserContextHandle.class), any(GURL.class));
        openPageInfo();
        onView(withId(PageInfoAboutThisSiteController.ROW_ID)).check(matches(not(isDisplayed())));
    }
}