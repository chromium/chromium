// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withParent;

import static org.hamcrest.core.AllOf.allOf;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.MockitoAnnotations.initMocks;

import android.app.Activity;
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
import org.mockito.Spy;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.SharedPreferencesManager;
import org.chromium.chrome.browser.sync.SyncServiceFactory;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.R;
import org.chromium.chrome.test.util.ChromeRenderTestRule;
import org.chromium.chrome.test.util.RecentTabsPageTestUtils;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;
import org.chromium.chrome.test.util.browser.signin.SigninTestRule;
import org.chromium.chrome.test.util.browser.signin.SigninTestUtil;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.components.policy.test.annotations.Policies;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.content_public.common.ContentUrlConstants;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.mojom.WindowOpenDisposition;
import org.chromium.url.GURL;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;
import java.util.concurrent.ExecutionException;

/**
 * Instrumentation tests for {@link RecentTabsPage}.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@EnableFeatures(ChromeFeatureList.EMPTY_STATES)
public class RecentTabsPageTest {
    private static final String EMAIL = "email@gmail.com";
    private static final String NAME = "Email Emailson";
    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    // FakeAccountInfoService is required to create the ProfileDataCache entry with sync_off badge
    // for Sync promo.
    @Rule
    public final SigninTestRule mSigninTestRule = new SigninTestRule();

    @Rule
    public final ChromeRenderTestRule mRenderTestRule =
            ChromeRenderTestRule.Builder.withPublicCorpus()
                    .setRevision(7)
                    .setBugComponent(ChromeRenderTestRule.Component.UI_BROWSER_MOBILE_RECENT_TABS)
                    .build();

    @Spy
    private FakeRecentlyClosedTabManager mManager = new FakeRecentlyClosedTabManager();
    private ChromeTabbedActivity mActivity;
    private Tab mTab;
    private TabModel mTabModel;
    private RecentTabsPage mPage;

    @Before
    public void setUp() throws Exception {
        initMocks(this);

        RecentTabsManager.setRecentlyClosedTabManagerForTests(mManager);
        mActivityTestRule.startMainActivityOnBlankPage();
        mActivity = mActivityTestRule.getActivity();
        mTabModel = mActivity.getTabModelSelector().getModel(false);
        mTab = mActivity.getActivityTab();
    }

    @After
    public void tearDown() {
        leaveRecentTabsPage();
        SharedPreferencesManager.getInstance().removeKey(
                ChromePreferenceKeys.SYNC_PROMO_TOTAL_SHOW_COUNT);
    }

    @Test
    @MediumTest
    @Feature({"RecentTabsPage"})
    public void testRecentlyClosedTabs() throws ExecutionException {
        mPage = loadRecentTabsPage();
        // Set a recently closed tab and confirm a view is rendered for it.
        final RecentlyClosedTab tab = new RecentlyClosedTab(
                0, 0, "Tab Title", new GURL("https://www.example.com/"), null);
        setRecentlyClosedEntries(Collections.singletonList(tab));
        Assert.assertEquals(1, mManager.getRecentlyClosedEntries(1).size());
        final String title = tab.getTitle();
        final View view = waitForView(title);

        openContextMenuAndInvokeItem(
                mActivity, view, RecentTabsRowAdapter.RecentlyClosedTabsGroup.ID_OPEN_IN_NEW_TAB);
        verify(mManager, times(1))
                .openRecentlyClosedTab(mTabModel, tab, WindowOpenDisposition.NEW_BACKGROUND_TAB);

        final int groupIdx = !DeviceFormFactor.isNonMultiDisplayContextOnTablet(mActivity) ? 0 : 1;
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { mPage.onChildClick(null, null, groupIdx, 0, 0); });
        verify(mManager, times(1))
                .openRecentlyClosedTab(mTabModel, tab, WindowOpenDisposition.CURRENT_TAB);

        // Clear the recently closed tabs with the context menu and confirm the view is gone.
        openContextMenuAndInvokeItem(
                mActivity, view, RecentTabsRowAdapter.RecentlyClosedTabsGroup.ID_REMOVE_ALL);
        Assert.assertEquals(0, mManager.getRecentlyClosedEntries(1).size());
        waitForViewToDisappear(title);
    }

    @Test
    @LargeTest
    @Feature({"RecentTabsPage", "RenderTest"})
    // Disable sign-in to suppress sync promo, as it's unrelated to this render test.
    @Policies.Add(@Policies.Item(key = "BrowserSignin", string = "0"))
    public void testRecentlyClosedGroup_WithTitle() throws Exception {
        mPage = loadRecentTabsPage();
        // Set a recently closed group and confirm a view is rendered for it.
        final RecentlyClosedGroup group = new RecentlyClosedGroup(2, 0, "Group Title");
        group.getTabs().add(new RecentlyClosedTab(
                0, 0, "Tab Title 0", new GURL("https://www.example.com/url/0"), "group1"));
        group.getTabs().add(new RecentlyClosedTab(
                1, 0, "Tab Title 1", new GURL("https://www.example.com/url/1"), "group1"));
        setRecentlyClosedEntries(Collections.singletonList(group));
        Assert.assertEquals(1, mManager.getRecentlyClosedEntries(1).size());
        final String groupString = TestThreadUtils.runOnUiThreadBlockingNoException(() -> {
            return mActivity.getResources().getString(
                    R.string.recent_tabs_group_closure_with_title, group.getTitle());
        });
        final View view = waitForView(groupString);

        mRenderTestRule.render(mPage.getView(), "recently_closed_group_with_title");

        final int groupIdx = !DeviceFormFactor.isNonMultiDisplayContextOnTablet(mActivity) ? 0 : 1;
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { mPage.onChildClick(null, null, groupIdx, 0, 0); });
        verify(mManager, times(1)).openRecentlyClosedEntry(mTabModel, group);

        // Clear the recently closed tabs with the context menu and confirm the view is gone.
        openContextMenuAndInvokeItem(
                mActivity, view, RecentTabsRowAdapter.RecentlyClosedTabsGroup.ID_REMOVE_ALL);
        Assert.assertEquals(0, mManager.getRecentlyClosedEntries(1).size());
        waitForViewToDisappear(groupString);
    }

    @Test
    @LargeTest
    @Feature({"RecentTabsPage", "RenderTest"})
    // Disable sign-in to suppress sync promo, as it's unrelated to this render test.
    @Policies.Add(@Policies.Item(key = "BrowserSignin", string = "0"))
    public void testRecentlyClosedGroup_WithoutTitle() throws Exception {
        mPage = loadRecentTabsPage();
        long time = 904881600000L;
        // Set a recently closed group and confirm a view is rendered for it.
        final RecentlyClosedGroup group = new RecentlyClosedGroup(2, time, null);
        group.getTabs().add(new RecentlyClosedTab(
                0, time, "Tab Title 0", new GURL("https://www.example.com/url/0"), "group1"));
        group.getTabs().add(new RecentlyClosedTab(
                1, time, "Tab Title 1", new GURL("https://www.example.com/url/1"), "group1"));
        setRecentlyClosedEntries(Collections.singletonList(group));
        Assert.assertEquals(1, mManager.getRecentlyClosedEntries(1).size());
        final String groupString = TestThreadUtils.runOnUiThreadBlockingNoException(() -> {
            return mActivity.getResources().getString(
                    R.string.recent_tabs_group_closure_without_title, group.getTabs().size());
        });
        final View view = waitForView(groupString);

        mRenderTestRule.render(mPage.getView(), "recently_closed_group_without_title");

        final int groupIdx = !DeviceFormFactor.isNonMultiDisplayContextOnTablet(mActivity) ? 0 : 1;
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { mPage.onChildClick(null, null, groupIdx, 0, 0); });
        verify(mManager, times(1)).openRecentlyClosedEntry(mTabModel, group);

        // Clear the recently closed tabs with the context menu and confirm the view is gone.
        openContextMenuAndInvokeItem(
                mActivity, view, RecentTabsRowAdapter.RecentlyClosedTabsGroup.ID_REMOVE_ALL);
        Assert.assertEquals(0, mManager.getRecentlyClosedEntries(1).size());
        waitForViewToDisappear(groupString);
    }

    @Test
    @LargeTest
    @Feature({"RecentTabsPage", "RenderTest"})
    // Disable sign-in to suppress sync promo, as it's unrelated to this render test.
    @Policies.Add(@Policies.Item(key = "BrowserSignin", string = "0"))
    public void testRecentlyClosedBulkEvent() throws Exception {
        mPage = loadRecentTabsPage();
        long time = 904881600000L;
        // Set a recently closed bulk event and confirm a view is rendered for it.
        final RecentlyClosedBulkEvent event = new RecentlyClosedBulkEvent(3, time);
        event.getGroupIdToTitleMap().put("group1", "Group 1 Title");
        event.getTabs().add(new RecentlyClosedTab(
                0, time, "Tab Title 0", new GURL("https://www.example.com/url/0"), "group1"));
        event.getTabs().add(new RecentlyClosedTab(
                1, time, "Tab Title 1", new GURL("https://www.example.com/url/1"), "group1"));
        event.getTabs().add(new RecentlyClosedTab(
                2, time, "Tab Title 2", new GURL("https://www.example.com/url/2"), null));
        setRecentlyClosedEntries(Collections.singletonList(event));
        Assert.assertEquals(1, mManager.getRecentlyClosedEntries(1).size());
        final int size = event.getTabs().size();
        final String eventString = TestThreadUtils.runOnUiThreadBlockingNoException(() -> {
            return mActivity.getResources().getString(R.string.recent_tabs_bulk_closure, size);
        });
        final View view = waitForView(eventString);

        mRenderTestRule.render(mPage.getView(), "recently_closed_bulk_event");

        final int groupIdx = !DeviceFormFactor.isNonMultiDisplayContextOnTablet(mActivity) ? 0 : 1;
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { mPage.onChildClick(null, null, groupIdx, 0, 0); });
        verify(mManager, times(1)).openRecentlyClosedEntry(mTabModel, event);

        // Clear the recently closed tabs with the context menu and confirm the view is gone.
        openContextMenuAndInvokeItem(
                mActivity, view, RecentTabsRowAdapter.RecentlyClosedTabsGroup.ID_REMOVE_ALL);
        Assert.assertEquals(0, mManager.getRecentlyClosedEntries(1).size());
        waitForViewToDisappear(eventString);
    }

    @Test
    @MediumTest
    @Feature({"RecentTabsPage"})
    public void testEmptyStateView() throws ExecutionException {
        // Sign in and enable sync.
        CoreAccountInfo coreAccountInfo = addAccountWithNonDisplayableEmail(NAME);
        SigninTestUtil.signinAndEnableSync(coreAccountInfo,
                TestThreadUtils.runOnUiThreadBlockingNoException(SyncServiceFactory::get));

        // Open an empty recent tabs page and confirm empty view shows.
        mPage = loadRecentTabsPage();
        onView(allOf(withId(R.id.empty_state_container),
                       withParent(withId(R.id.legacy_sync_promo_view_frame_layout))))
                .check(matches(isDisplayed()));
    }

    private CoreAccountInfo addAccountWithNonDisplayableEmail(String name) {
        CoreAccountInfo coreAccountInfo = mSigninTestRule.addAccount(
                EMAIL, name, SigninTestRule.NON_DISPLAYABLE_EMAIL_ACCOUNT_CAPABILITIES);
        mSigninTestRule.waitForSeeding();
        return coreAccountInfo;
    }

    /**
     * Generates the specified number of {@link RecentlyClosedTab} instances and sets them on the
     * manager.
     */
    private void setRecentlyClosedEntries(List<RecentlyClosedEntry> entries) {
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { mManager.setRecentlyClosedEntries(entries); });
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

    private static void openContextMenuAndInvokeItem(
            final Activity activity, final View view, final int itemId) {
        // IMPLEMENTATION NOTE: Instrumentation.invokeContextMenuAction would've been much simpler,
        // but it requires the View to be focused which is hard to achieve in touch mode.
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            view.performLongClick();
            activity.getWindow().performContextMenuIdentifierAction(itemId, 0);
        });
    }
}
