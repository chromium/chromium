// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.page_info;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.CoreMatchers.allOf;
import static org.hamcrest.CoreMatchers.containsString;
import static org.hamcrest.CoreMatchers.not;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.verify;

import static org.chromium.ui.test.util.ViewUtils.onViewWaiting;

import android.annotation.SuppressLint;

import androidx.annotation.NonNull;
import androidx.test.espresso.ViewAssertion;
import androidx.test.filters.MediumTest;

import org.junit.Before;
import org.junit.ClassRule;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.ephemeraltab.EphemeralTabCoordinator;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabbed_mode.TabbedRootUiCoordinator;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.R;
import org.chromium.chrome.test.batch.BlankCTATabInitialStateRule;
import org.chromium.chrome.test.util.ChromeRenderTestRule;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetTestSupport;
import org.chromium.components.page_info.PageInfoController;
import org.chromium.components.page_info.proto.AboutThisSiteMetadataProto.Hyperlink;
import org.chromium.components.page_info.proto.AboutThisSiteMetadataProto.MoreAbout;
import org.chromium.components.page_info.proto.AboutThisSiteMetadataProto.SiteDescription;
import org.chromium.components.page_info.proto.AboutThisSiteMetadataProto.SiteInfo;
import org.chromium.content_public.browser.BrowserContextHandle;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.common.ContentSwitches;
import org.chromium.net.test.EmbeddedTestServerRule;
import org.chromium.url.GURL;

import java.io.IOException;
import java.util.concurrent.TimeoutException;

/** Tests for PageInfoAboutThisSite. */
@RunWith(ChromeJUnit4ClassRunner.class)
@EnableFeatures({ChromeFeatureList.PAGE_INFO_ABOUT_THIS_SITE_MORE_LANGS})
@CommandLineFlags.Add({
    ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE,
    ChromeSwitches.DISABLE_STARTUP_PROMOS,
    ContentSwitches.HOST_RESOLVER_RULES + "=MAP * 127.0.0.1",
    "ignore-certificate-errors"
})
@Batch(Batch.PER_CLASS)
@SuppressLint("VisibleForTests")
public class PageInfoAboutThisSiteTest {
    private static final String sSimpleHtml = "/chrome/test/data/android/simple.html";
    private static final String sAboutHtml = "/chrome/test/data/android/about.html";

    @ClassRule
    public static final ChromeTabbedActivityTestRule sActivityTestRule =
            new ChromeTabbedActivityTestRule();

    @Rule
    public final BlankCTATabInitialStateRule mInitialStateRule =
            new BlankCTATabInitialStateRule(sActivityTestRule, false);

    @Rule public EmbeddedTestServerRule mTestServerRule = new EmbeddedTestServerRule();

    @Rule public JniMocker mMocker = new JniMocker();

    @Rule
    public ChromeRenderTestRule mRenderTestRule =
            ChromeRenderTestRule.Builder.withPublicCorpus()
                    .setBugComponent(ChromeRenderTestRule.Component.UI_BROWSER_BUBBLES_PAGE_INFO)
                    .build();

    private EphemeralTabCoordinator mEphemeralTabCoordinator;

    private BottomSheetTestSupport mSheetTestSupport;

    @Mock private PageInfoAboutThisSiteController.Natives mMockAboutThisSiteJni;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        doReturn(true).when(mMockAboutThisSiteJni).isFeatureEnabled();
        doReturn(R.drawable.ic_info_outline_grey_24dp)
                .when(mMockAboutThisSiteJni)
                .getJavaDrawableIconId();
        mMocker.mock(PageInfoAboutThisSiteControllerJni.TEST_HOOKS, mMockAboutThisSiteJni);
        mTestServerRule.setServerUsesHttps(true);
        sActivityTestRule.loadUrl(mTestServerRule.getServer().getURL(sSimpleHtml));

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    TabbedRootUiCoordinator tabbedRootUiCoordinator =
                            ((TabbedRootUiCoordinator)
                                    sActivityTestRule
                                            .getActivity()
                                            .getRootUiCoordinatorForTesting());
                    mEphemeralTabCoordinator =
                            tabbedRootUiCoordinator.getEphemeralTabCoordinatorSupplier().get();
                });

        mSheetTestSupport =
                new BottomSheetTestSupport(
                        sActivityTestRule
                                .getActivity()
                                .getRootUiCoordinatorForTesting()
                                .getBottomSheetController());
    }

    private void openPageInfo() {
        ChromeTabbedActivity activity = sActivityTestRule.getActivity();
        Tab tab = activity.getActivityTab();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    new ChromePageInfo(
                                    activity.getModalDialogManagerSupplier(),
                                    null,
                                    PageInfoController.OpenedFromSource.TOOLBAR,
                                    null,
                                    () -> mEphemeralTabCoordinator,
                                    null)
                            .show(tab, ChromePageInfoHighlight.noHighlight());
                });
        onViewWaiting(allOf(withId(R.id.page_info_url_wrapper), isDisplayed()), true);
    }

    private void dismissPageInfo() throws TimeoutException {
        CallbackHelper helper = new CallbackHelper();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    PageInfoController.getLastPageInfoControllerForTesting()
                            .runAfterDismiss(helper::notifyCalled);
                });
        helper.waitForCallback(0);
    }

    /**
     * End all animations that already started before so that the UI will be in a state ready for
     * the next command.
     */
    private void endAnimations() {
        ThreadUtils.runOnUiThreadBlocking(mSheetTestSupport::endAllAnimations);
    }

    private void closeBottomSheet() {
        ThreadUtils.runOnUiThreadBlocking(mEphemeralTabCoordinator::close);
        endAnimations();
        assertFalse(
                "The bottomsheet should have closed but did not indicate closed",
                mEphemeralTabCoordinator.isOpened());
    }

    private @NonNull ViewAssertion renderView(String renderId) {
        return (v, noMatchException) -> {
            if (noMatchException != null) throw noMatchException;
            try {
                mRenderTestRule.render(v, renderId);
            } catch (IOException e) {
                throw new RuntimeException(e);
            }
        };
    }

    private void mockResponse(SiteInfo.Builder builder) {
        doReturn(builder != null ? builder.build().toByteArray() : null)
                .when(mMockAboutThisSiteJni)
                .getSiteInfo(
                        any(BrowserContextHandle.class), any(GURL.class), any(WebContents.class));
    }

    private SiteInfo.Builder createDescription() {
        String url = mTestServerRule.getServer().getURL(sSimpleHtml);
        String moreAboutUrl = mTestServerRule.getServer().getURL(sAboutHtml);
        SiteDescription.Builder description =
                SiteDescription.newBuilder()
                        .setDescription("Some description about example.com for testing purposes")
                        .setSource(Hyperlink.newBuilder().setUrl(url).setLabel("Example Source"));
        MoreAbout.Builder moreAbout = MoreAbout.newBuilder().setUrl(moreAboutUrl);
        return SiteInfo.newBuilder().setDescription(description).setMoreAbout(moreAbout);
    }

    @Test
    @MediumTest
    public void testAboutThisSiteRowWithData() throws TimeoutException {
        mockResponse(createDescription());
        openPageInfo();
        onView(withId(PageInfoAboutThisSiteController.ROW_ID)).check(matches(isDisplayed()));
        onView(withText(containsString("Some description"))).check(matches(isDisplayed()));
        dismissPageInfo();
    }

    @Test
    @MediumTest
    public void testAboutThisSiteRowWithDataOnInsecureSite() {
        sActivityTestRule.loadUrl(
                mTestServerRule.getServer().getURLWithHostName("invalidcert.com", sSimpleHtml));
        mockResponse(createDescription());
        openPageInfo();
        onView(withId(PageInfoAboutThisSiteController.ROW_ID)).check(matches(not(isDisplayed())));
    }

    @Test
    @MediumTest
    public void testAboutThisSiteRowWithoutData() throws TimeoutException {
        mockResponse(null);
        openPageInfo();
        onView(withId(PageInfoAboutThisSiteController.ROW_ID)).check(matches(not(isDisplayed())));
        dismissPageInfo();
    }

    @Test
    @MediumTest
    public void testAboutThisSiteRowWithoutDescription() throws TimeoutException {
        mockResponse(createDescription().clearDescription());
        openPageInfo();
        onView(withId(PageInfoAboutThisSiteController.ROW_ID)).check(matches(isDisplayed()));
        dismissPageInfo();
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testAboutThisSiteRowRendering() {
        mockResponse(createDescription());
        openPageInfo();
        onView(withId(PageInfoAboutThisSiteController.ROW_ID))
                .check(renderView("page_info_about_this_site_row"));
    }

    @Test
    @MediumTest
    @Feature({"RenderTest"})
    public void testAboutThisSiteOpensEphemeralTabWithImprovedBottomSheetEnabled() {
        mockResponse(createDescription());
        openPageInfo();

        onView(withId(PageInfoAboutThisSiteController.ROW_ID)).perform(click());
        verify(mMockAboutThisSiteJni).onAboutThisSiteRowClicked(true);
        endAnimations();
        assertTrue("The bottomsheet did not open", mEphemeralTabCoordinator.isOpened());

        String moreAboutUrl = mTestServerRule.getServer().getURL(sAboutHtml);
        assertEquals(
                new GURL(moreAboutUrl + "?ilrm=minimal%2Cnohead"),
                mEphemeralTabCoordinator.getUrlForTesting());
        assertEquals(new GURL(moreAboutUrl), mEphemeralTabCoordinator.getFullPageUrlForTesting());

        onView(withId(R.id.bottom_sheet))
                .check(renderView("page_info_about_this_site_improved_bottomsheet"));

        closeBottomSheet();
    }

    @Test
    @MediumTest
    public void testAboutThisSiteWithoutDescription() {
        mockResponse(createDescription().clearDescription());
        openPageInfo();

        onView(withId(PageInfoAboutThisSiteController.ROW_ID)).perform(click());
        endAnimations();
        assertTrue("The bottomsheet did not open", mEphemeralTabCoordinator.isOpened());

        verify(mMockAboutThisSiteJni).onAboutThisSiteRowClicked(false);

        closeBottomSheet();
    }
}
