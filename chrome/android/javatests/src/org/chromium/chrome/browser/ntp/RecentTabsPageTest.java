// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp;

import android.accounts.Account;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
import android.support.test.InstrumentationRegistry;
import android.view.View;

import androidx.test.filters.LargeTest;
import androidx.test.filters.MediumTest;

import org.hamcrest.Matchers;
import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.FlakyTest;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.ChromeRenderTestRule;
import org.chromium.chrome.test.util.RecentTabsPageTestUtils;
import org.chromium.chrome.test.util.browser.signin.AccountManagerTestRule;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.components.signin.ProfileDataSource;
import org.chromium.components.signin.test.util.FakeProfileDataSource;
import org.chromium.content_public.browser.test.util.Criteria;
import org.chromium.content_public.browser.test.util.CriteriaHelper;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.content_public.browser.test.util.TestTouchUtils;
import org.chromium.content_public.common.ContentUrlConstants;
import org.chromium.url.GURL;

import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.ExecutionException;

/**
 * Instrumentation tests for {@link RecentTabsPage}.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class RecentTabsPageTest {
    // FakeProfileDataSource is required to create the ProfileDataCache entry with sync_off badge
    // for Sync promo.
    private final FakeProfileDataSource mFakeProfileDataSource = new FakeProfileDataSource();

    @Rule
    public final AccountManagerTestRule mAccountManagerTestRule =
            new AccountManagerTestRule(mFakeProfileDataSource);

    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    @Rule
    public final ChromeRenderTestRule mRenderTestRule =
            ChromeRenderTestRule.Builder.withPublicCorpus().build();

    private FakeRecentlyClosedTabManager mManager;
    private Tab mTab;
    private RecentTabsPage mPage;

    @Before
    public void setUp() throws Exception {
        mManager = new FakeRecentlyClosedTabManager();
        RecentTabsManager.setRecentlyClosedTabManagerForTests(mManager);
        mActivityTestRule.startMainActivityOnBlankPage();
        mTab = mActivityTestRule.getActivity().getActivityTab();
    }

    @After
    public void tearDown() {
        leaveRecentTabsPage();
        RecentTabsManager.forcePromoStateForTests(null);
        RecentTabsManager.setRecentlyClosedTabManagerForTests(null);
    }

    @Test
    @MediumTest
    @Feature({"RecentTabsPage"})
    @FlakyTest(message = "crbug.com/1075804")
    public void testRecentlyClosedTabs() throws ExecutionException {
        mPage = loadRecentTabsPage();
        // Set a recently closed tab and confirm a view is rendered for it.
        List<RecentlyClosedTab> tabs = setRecentlyClosedTabs(1);
        Assert.assertEquals(1, mManager.getRecentlyClosedTabs(1).size());
        String title = tabs.get(0).title;
        View view = waitForView(title);

        // Clear the recently closed tabs with the context menu and confirm the view is gone.
        invokeContextMenu(view, RecentTabsRowAdapter.RecentlyClosedTabsGroup.ID_REMOVE_ALL);
        Assert.assertEquals(0, mManager.getRecentlyClosedTabs(1).size());
        waitForViewToDisappear(title);
    }

    @Test
    @LargeTest
    @Feature("RenderTest")
    public void testPersonalizedSigninPromoInRecentTabsPage() throws Exception {
        Account account = mAccountManagerTestRule.addTestAccountThenSigninAndEnableSync();
        TestThreadUtils.runOnUiThreadBlocking(
                ()
                        -> mFakeProfileDataSource.setProfileData(account.name,
                                new ProfileDataSource.ProfileData(
                                        account.name, createAvatar(), "Full Name", "Given Name")));
        RecentTabsManager.forcePromoStateForTests(
                RecentTabsManager.PromoState.PROMO_SIGNIN_PERSONALIZED);
        mPage = loadRecentTabsPage();
        mRenderTestRule.render(mPage.getView(), "personalized_signin_promo_recent_tabs_page");
    }

    @Test
    @LargeTest
    @Feature("RenderTest")
    public void testPersonalizedSyncPromoInRecentTabsPage() throws Exception {
        Account account = mAccountManagerTestRule.addTestAccountThenSigninAndEnableSync();
        TestThreadUtils.runOnUiThreadBlocking(
                ()
                        -> mFakeProfileDataSource.setProfileData(account.name,
                                new ProfileDataSource.ProfileData(
                                        account.name, createAvatar(), "Full Name", "Given Name")));
        RecentTabsManager.forcePromoStateForTests(
                RecentTabsManager.PromoState.PROMO_SYNC_PERSONALIZED);
        mPage = loadRecentTabsPage();
        mRenderTestRule.render(mPage.getView(), "personalized_sync_promo_recent_tabs_page");
    }

    /**
     * Generates the specified number of {@link RecentlyClosedTab} instances and sets them on the
     * manager.
     */
    private List<RecentlyClosedTab> setRecentlyClosedTabs(final int tabCount) {
        final List<RecentlyClosedTab> tabs = new ArrayList<>();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            for (int i = 0; i < tabCount; i++) {
                tabs.add(new RecentlyClosedTab(i, "RecentlyClosedTab title " + i,
                        new GURL("https://www.example.com/url" + i)));
            }
            mManager.setRecentlyClosedTabs(tabs);
        });
        return tabs;
    }

    private RecentTabsPage loadRecentTabsPage() {
        mActivityTestRule.loadUrl(UrlConstants.RECENT_TABS_URL);
        RecentTabsPageTestUtils.waitForRecentTabsPageLoaded(mTab);
        return (RecentTabsPage) mTab.getNativePage();
    }

    /**
     * Leaves and destroys the {@link RecentTabsPage} by navigating the tab to {@code about:blank}.
     */
    private void leaveRecentTabsPage() {
        mActivityTestRule.loadUrl(ContentUrlConstants.ABOUT_BLANK_DISPLAY_URL);
        CriteriaHelper.pollUiThread(() -> {
            Criteria.checkThat("RecentTabsPage is still there", mTab.getNativePage(),
                    Matchers.not(Matchers.instanceOf(RecentTabsPage.class)));
        });
    }

    /**
     * Waits for the view with the specified text to appear.
     */
    private View waitForView(final String text) {
        final ArrayList<View> views = new ArrayList<>();
        CriteriaHelper.pollUiThread(() -> {
            mPage.getView().findViewsWithText(views, text, View.FIND_VIEWS_WITH_TEXT);
            Criteria.checkThat(
                    "Could not find view with this text: " + text, views.size(), Matchers.is(1));
        });
        return views.get(0);
    }

    /**
     * Waits for the view with the specified text to disappear.
     */
    private void waitForViewToDisappear(final String text) {
        CriteriaHelper.pollUiThread(() -> {
            ArrayList<View> views = new ArrayList<>();
            mPage.getView().findViewsWithText(views, text, View.FIND_VIEWS_WITH_TEXT);
            Criteria.checkThat(
                    "View with this text is still present: " + text, views, Matchers.empty());
        });
    }

    private void invokeContextMenu(View view, int contextMenuItemId) throws ExecutionException {
        TestTouchUtils.performLongClickOnMainSync(
                InstrumentationRegistry.getInstrumentation(), view);
        Assert.assertTrue(InstrumentationRegistry.getInstrumentation().invokeContextMenuAction(
                mActivityTestRule.getActivity(), contextMenuItemId, 0));
    }

    /**
     * TODO(https://crbug.com/1125452): Remove this method and use test only resource.
     */
    private Bitmap createAvatar() {
        final int avatarSize = 40;

        Bitmap result = Bitmap.createBitmap(avatarSize, avatarSize, Bitmap.Config.ARGB_8888);
        Canvas canvas = new Canvas(result);
        canvas.drawColor(Color.RED);

        Paint paint = new Paint();
        paint.setAntiAlias(true);

        paint.setColor(Color.BLUE);
        canvas.drawCircle(0, 0, avatarSize, paint);

        return result;
    }
}
