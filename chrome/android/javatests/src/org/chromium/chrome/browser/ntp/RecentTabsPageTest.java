// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withParent;

import static org.hamcrest.CoreMatchers.instanceOf;
import static org.hamcrest.MatcherAssert.assertThat;
import static org.hamcrest.core.AllOf.allOf;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.MockitoAnnotations.initMocks;

import android.app.Activity;
import android.content.res.ColorStateList;
import android.content.res.Resources;
import android.graphics.drawable.GradientDrawable;
import android.view.View;
import android.widget.ImageView;
import android.widget.TextView;

import androidx.annotation.StringRes;
import androidx.test.filters.LargeTest;
import androidx.test.filters.MediumTest;
import androidx.test.filters.SmallTest;

import org.hamcrest.Matchers;
import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Spy;

import org.chromium.base.ThreadUtils;
import org.chromium.base.Token;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tasks.tab_management.ColorPickerUtils;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.R;
import org.chromium.chrome.test.util.ChromeRenderTestRule;
import org.chromium.chrome.test.util.RecentTabsPageTestUtils;
import org.chromium.chrome.test.util.browser.signin.AccountManagerTestRule;
import org.chromium.chrome.test.util.browser.signin.SigninTestRule;
import org.chromium.chrome.test.util.browser.signin.SigninTestUtil;
import org.chromium.chrome.test.util.browser.sync.SyncTestUtil;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.components.embedder_support.util.UrlUtilities;
import org.chromium.components.policy.test.annotations.Policies;
import org.chromium.components.tab_groups.TabGroupColorId;
import org.chromium.content_public.common.ContentUrlConstants;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.mojom.WindowOpenDisposition;
import org.chromium.url.GURL;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;
import java.util.concurrent.ExecutionException;

/** Instrumentation tests for {@link RecentTabsPage}. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@EnableFeatures({ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS})
public class RecentTabsPageTest {
    private static final String EMAIL = "email@gmail.com";
    private static final String NAME = "Email Emailson";
    private static final int COLOR_ID = TabGroupColorId.YELLOW;
    private static final int COLOR_ID_2 = TabGroupColorId.RED;

    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    // FakeAccountInfoService is required to create the ProfileDataCache entry with sync_off badge
    // for Sync promo.
    @Rule public final SigninTestRule mSigninTestRule = new SigninTestRule();

    @Rule
    public final ChromeRenderTestRule mRenderTestRule =
            ChromeRenderTestRule.Builder.withPublicCorpus()
                    .setRevision(9)
                    .setBugComponent(ChromeRenderTestRule.Component.UI_BROWSER_MOBILE_RECENT_TABS)
                    .build();

    @Spy private FakeRecentlyClosedTabManager mManager = new FakeRecentlyClosedTabManager();
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
        ChromeSharedPreferences.getInstance()
                .removeKey(ChromePreferenceKeys.SYNC_PROMO_TOTAL_SHOW_COUNT);
    }

    @Test
    @MediumTest
    @Feature({"RecentTabsPage"})
    @DisableFeatures({ChromeFeatureList.TAB_GROUP_PARITY_ANDROID})
    public void testRecentlyClosedGroupNoColorContentDescriptions() throws ExecutionException {
        mPage = loadRecentTabsPage();
        // Set a recently closed group with a title and confirm a view is rendered for it.
        final RecentlyClosedGroup titleGroup =
                new RecentlyClosedGroup(2, 0, "Group Title", COLOR_ID);
        Token titleTabGroupId = new Token(27839L, 4789L);
        titleGroup
                .getTabs()
                .add(
                        new RecentlyClosedTab(
                                0,
                                0,
                                "Tab Title 0",
                                new GURL("https://www.example.com/url/0"),
                                titleTabGroupId));
        titleGroup
                .getTabs()
                .add(
                        new RecentlyClosedTab(
                                1,
                                0,
                                "Tab Title 1",
                                new GURL("https://www.example.com/url/1"),
                                titleTabGroupId));

        // Set a recently closed group without a title and confirm a view is rendered for it.
        final RecentlyClosedGroup noTitleGroup = new RecentlyClosedGroup(3, 0, null, COLOR_ID_2);
        Token noTitleTabGroupId = new Token(798L, 4389L);
        noTitleGroup
                .getTabs()
                .add(
                        new RecentlyClosedTab(
                                0,
                                0,
                                "Tab Title 0",
                                new GURL("https://www.example.com/url/0"),
                                noTitleTabGroupId));

        List<RecentlyClosedEntry> entries = new ArrayList<>();
        entries.add(titleGroup);
        entries.add(noTitleGroup);
        setRecentlyClosedEntries(entries);
        assertEquals(2, mManager.getRecentlyClosedEntries(2).size());
        final String titleGroupString =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> {
                            return mActivity
                                    .getResources()
                                    .getString(
                                            R.string.recent_tabs_group_closure_with_title,
                                            titleGroup.getTitle());
                        });
        final String titleGroupAccessibilityString =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> {
                            return mActivity
                                    .getResources()
                                    .getString(
                                            R.string
                                                    .recent_tabs_group_closure_with_title_accessibility,
                                            titleGroup.getTitle());
                        });
        final String noTitleGroupString =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> {
                            return mActivity
                                    .getResources()
                                    .getQuantityString(
                                            R.plurals.recent_tabs_group_closure_without_title,
                                            noTitleGroup.getTabs().size(),
                                            noTitleGroup.getTabs().size());
                        });
        final String noTitleGroupAccessibilityString =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> {
                            return mActivity
                                    .getResources()
                                    .getQuantityString(
                                            R.plurals
                                                    .recent_tabs_group_closure_without_title_accessibility,
                                            noTitleGroup.getTabs().size(),
                                            noTitleGroup.getTabs().size());
                        });
        final View titleGroupView = waitForView(titleGroupString);
        final View noTitleGroupView = waitForTabCountTitleView(noTitleGroupString);
        assertEquals(titleGroupAccessibilityString, titleGroupView.getContentDescription());
        assertEquals(noTitleGroupAccessibilityString, noTitleGroupView.getContentDescription());
    }

    @Test
    @MediumTest
    @Feature({"RecentTabsPage"})
    public void testRecentlyClosedTabs() throws ExecutionException {
        mPage = loadRecentTabsPage();
        // Set a recently closed tab and confirm a view is rendered for it.
        final RecentlyClosedTab tab =
                new RecentlyClosedTab(
                        0, 0, "Tab Title", new GURL("https://www.example.com/"), null);
        setRecentlyClosedEntries(Collections.singletonList(tab));
        assertEquals(1, mManager.getRecentlyClosedEntries(1).size());
        final String title = tab.getTitle();
        final View view = waitForView(title);

        openContextMenuAndInvokeItem(
                mActivity, view, RecentTabsRowAdapter.RecentlyClosedTabsGroup.ID_OPEN_IN_NEW_TAB);
        verify(mManager, times(1))
                .openRecentlyClosedTab(mTabModel, tab, WindowOpenDisposition.NEW_BACKGROUND_TAB);

        final int groupIdx = !DeviceFormFactor.isNonMultiDisplayContextOnTablet(mActivity) ? 0 : 1;
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mPage.onChildClick(null, null, groupIdx, 0, 0);
                });
        verify(mManager, times(1))
                .openRecentlyClosedTab(mTabModel, tab, WindowOpenDisposition.CURRENT_TAB);

        // Clear the recently closed tabs with the context menu and confirm the view is gone.
        openContextMenuAndInvokeItem(
                mActivity, view, RecentTabsRowAdapter.RecentlyClosedTabsGroup.ID_REMOVE_ALL);
        assertEquals(0, mManager.getRecentlyClosedEntries(1).size());
        waitForViewToDisappear(title);
    }

    @Test
    @MediumTest
    @Feature({"RecentTabsPage"})
    @EnableFeatures({ChromeFeatureList.TAB_GROUP_PARITY_ANDROID})
    public void testRecentlyClosedGroupColor() throws ExecutionException {
        mPage = loadRecentTabsPage();
        // Set a recently closed group and confirm a view is rendered for it.
        final RecentlyClosedGroup group = new RecentlyClosedGroup(2, 0, "Group Title", COLOR_ID);

        setRecentlyClosedEntries(Collections.singletonList(group));
        assertEquals(1, mManager.getRecentlyClosedEntries(1).size());
        final String title = group.getTitle();
        final View view = waitForView(title);

        ImageView iconView = (ImageView) mPage.getView().findViewById(R.id.row_icon);
        assertNotNull(iconView.getBackground());
        assertEquals(View.VISIBLE, iconView.getVisibility());
        assertThat(iconView.getBackground(), instanceOf(GradientDrawable.class));

        GradientDrawable bgDrawable = (GradientDrawable) iconView.getBackground();
        assertEquals(GradientDrawable.OVAL, bgDrawable.getShape());
        assertEquals(
                ColorStateList.valueOf(
                        ColorPickerUtils.getTabGroupColorPickerItemColor(
                                mActivity, COLOR_ID, /* isIncognito= */ false)),
                bgDrawable.getColor());
    }

    @Test
    @MediumTest
    @Feature({"RecentTabsPage"})
    @EnableFeatures({ChromeFeatureList.TAB_GROUP_PARITY_ANDROID})
    public void testRecentlyClosedGroupIconDoesNotPersist() throws ExecutionException {
        mPage = loadRecentTabsPage();
        // Set a recently closed group and confirm a view is rendered for it.
        final RecentlyClosedGroup group = new RecentlyClosedGroup(2, 0, "Group Title", COLOR_ID);

        setRecentlyClosedEntries(Collections.singletonList(group));
        assertEquals(1, mManager.getRecentlyClosedEntries(1).size());
        final String groupString =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> {
                            return mActivity
                                    .getResources()
                                    .getString(
                                            R.string.recent_tabs_group_closure_with_title,
                                            group.getTitle());
                        });
        final View view = waitForView(groupString);

        // Test clicking the group to simulate an open action.
        final int groupIdx = !DeviceFormFactor.isNonMultiDisplayContextOnTablet(mActivity) ? 0 : 1;
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mPage.onChildClick(null, null, groupIdx, 0, 0);
                });
        verify(mManager, times(1)).openRecentlyClosedEntry(mTabModel, group);

        // Clear the recently closed tabs with the context menu and confirm the view is gone.
        openContextMenuAndInvokeItem(
                mActivity, view, RecentTabsRowAdapter.RecentlyClosedTabsGroup.ID_REMOVE_ALL);
        assertEquals(0, mManager.getRecentlyClosedEntries(1).size());
        waitForViewToDisappear(groupString);

        // Check that the remaining show history row item does not have an icon visible.
        ImageView iconView = (ImageView) mPage.getView().findViewById(R.id.row_icon);
        assertEquals(View.GONE, iconView.getVisibility());
    }

    @Test
    @LargeTest
    @Feature({"RecentTabsPage", "RenderTest"})
    @EnableFeatures({ChromeFeatureList.TAB_GROUP_PARITY_ANDROID})
    // Disable sign-in to suppress sign-in promo, as it's unrelated to this render test.
    @Policies.Add(@Policies.Item(key = "BrowserSignin", string = "0"))
    public void testRecentlyClosedGroup_WithTitle() throws Exception {
        mPage = loadRecentTabsPage();
        // Set a recently closed group and confirm a view is rendered for it.
        final RecentlyClosedGroup group = new RecentlyClosedGroup(2, 0, "Group Title", COLOR_ID);
        Token tabGroupId = new Token(27839L, 4789L);
        group.getTabs()
                .add(
                        new RecentlyClosedTab(
                                0,
                                0,
                                "Tab Title 0",
                                new GURL("https://www.example.com/url/0"),
                                tabGroupId));
        group.getTabs()
                .add(
                        new RecentlyClosedTab(
                                1,
                                0,
                                "Tab Title 1",
                                new GURL("https://www.example.com/url/1"),
                                tabGroupId));
        setRecentlyClosedEntries(Collections.singletonList(group));
        assertEquals(1, mManager.getRecentlyClosedEntries(1).size());
        final String groupString =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> {
                            return mActivity
                                    .getResources()
                                    .getString(
                                            R.string.recent_tabs_group_closure_with_title,
                                            group.getTitle());
                        });
        final String groupAccessibilityString =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> {
                            Resources res = mActivity.getResources();
                            final @StringRes int colorDesc =
                                    ColorPickerUtils
                                            .getTabGroupColorPickerItemColorAccessibilityString(
                                                    group.getColor());
                            return res.getString(
                                    R.string
                                            .recent_tabs_group_closure_with_title_with_color_accessibility,
                                    group.getTitle(),
                                    res.getString(colorDesc));
                        });
        final View view = waitForView(groupString);
        assertEquals(groupAccessibilityString, view.getContentDescription());

        mRenderTestRule.render(mPage.getView(), "recently_closed_group_with_title");

        final int groupIdx = !DeviceFormFactor.isNonMultiDisplayContextOnTablet(mActivity) ? 0 : 1;
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mPage.onChildClick(null, null, groupIdx, 0, 0);
                });
        verify(mManager, times(1)).openRecentlyClosedEntry(mTabModel, group);

        // Clear the recently closed tabs with the context menu and confirm the view is gone.
        openContextMenuAndInvokeItem(
                mActivity, view, RecentTabsRowAdapter.RecentlyClosedTabsGroup.ID_REMOVE_ALL);
        assertEquals(0, mManager.getRecentlyClosedEntries(1).size());
        waitForViewToDisappear(groupString);
    }

    @Test
    @LargeTest
    @Feature({"RecentTabsPage", "RenderTest"})
    @EnableFeatures({ChromeFeatureList.TAB_GROUP_PARITY_ANDROID})
    // Disable sign-in to suppress sign-in promo, as it's unrelated to this render test.
    @Policies.Add(@Policies.Item(key = "BrowserSignin", string = "0"))
    public void testRecentlyClosedGroup_WithoutTitle() throws Exception {
        mPage = loadRecentTabsPage();
        long time = 904881600000L;
        // Set a recently closed group and confirm a view is rendered for it.
        final RecentlyClosedGroup group = new RecentlyClosedGroup(2, time, null, COLOR_ID);
        Token tabGroupId = new Token(798L, 4389L);
        group.getTabs()
                .add(
                        new RecentlyClosedTab(
                                0,
                                time,
                                "Tab Title 0",
                                new GURL("https://www.example.com/url/0"),
                                tabGroupId));
        group.getTabs()
                .add(
                        new RecentlyClosedTab(
                                1,
                                time,
                                "Tab Title 1",
                                new GURL("https://www.example.com/url/1"),
                                tabGroupId));
        setRecentlyClosedEntries(Collections.singletonList(group));
        assertEquals(1, mManager.getRecentlyClosedEntries(1).size());
        final String groupString =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> {
                            return mActivity
                                    .getResources()
                                    .getQuantityString(
                                            R.plurals.recent_tabs_group_closure_without_title,
                                            group.getTabs().size(),
                                            group.getTabs().size());
                        });
        final String groupAccessibilityString =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> {
                            Resources res = mActivity.getResources();
                            final @StringRes int colorDesc =
                                    ColorPickerUtils
                                            .getTabGroupColorPickerItemColorAccessibilityString(
                                                    group.getColor());
                            return res.getQuantityString(
                                    R.plurals
                                            .recent_tabs_group_closure_without_title_with_color_accessibility,
                                    group.getTabs().size(),
                                    group.getTabs().size(),
                                    res.getString(colorDesc));
                        });
        List<String> domainList = new ArrayList<>();
        for (RecentlyClosedTab tab : group.getTabs()) {
            String domain = UrlUtilities.getDomainAndRegistry(tab.getUrl().getSpec(), false);
            domainList.add(domain);
        }
        final String groupDomainString =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> {
                            return mActivity
                                    .getResources()
                                    .getQuantityString(
                                            R.plurals.recent_tabs_group_closure_domain_text,
                                            group.getTabs().size(),
                                            group.getTabs().size(),
                                            String.join(", ", domainList));
                        });
        final View view = waitForTabCountTitleView(groupString);
        assertEquals(groupAccessibilityString, view.getContentDescription());
        final TextView domainView = (TextView) waitForView(groupDomainString);
        assertEquals(groupDomainString, domainView.getText());

        mRenderTestRule.render(mPage.getView(), "recently_closed_group_without_title");

        final int groupIdx = !DeviceFormFactor.isNonMultiDisplayContextOnTablet(mActivity) ? 0 : 1;
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mPage.onChildClick(null, null, groupIdx, 0, 0);
                });
        verify(mManager, times(1)).openRecentlyClosedEntry(mTabModel, group);

        // Clear the recently closed tabs with the context menu and confirm the view is gone.
        openContextMenuAndInvokeItem(
                mActivity, view, RecentTabsRowAdapter.RecentlyClosedTabsGroup.ID_REMOVE_ALL);
        assertEquals(0, mManager.getRecentlyClosedEntries(1).size());
        waitForViewToDisappear(groupString);
    }

    @Test
    @LargeTest
    @Feature({"RecentTabsPage", "RenderTest"})
    // Disable sign-in to suppress sign-in promo, as it's unrelated to this render test.
    @Policies.Add(@Policies.Item(key = "BrowserSignin", string = "0"))
    public void testRecentlyClosedBulkEvent() throws Exception {
        mPage = loadRecentTabsPage();
        long time = 904881600000L;
        // Set a recently closed bulk event and confirm a view is rendered for it.
        final RecentlyClosedBulkEvent event = new RecentlyClosedBulkEvent(3, time);
        Token tabGroupId = new Token(1L, 2L);
        event.getTabGroupIdToTitleMap().put(tabGroupId, "Group 1 Title");
        event.getTabs()
                .add(
                        new RecentlyClosedTab(
                                0,
                                time,
                                "Tab Title 0",
                                new GURL("https://www.example.com/url/0"),
                                tabGroupId));
        event.getTabs()
                .add(
                        new RecentlyClosedTab(
                                1,
                                time,
                                "Tab Title 1",
                                new GURL("https://www.example.com/url/1"),
                                tabGroupId));
        event.getTabs()
                .add(
                        new RecentlyClosedTab(
                                2,
                                time,
                                "Tab Title 2",
                                new GURL("https://www.example.com/url/2"),
                                null));
        setRecentlyClosedEntries(Collections.singletonList(event));
        assertEquals(1, mManager.getRecentlyClosedEntries(1).size());
        final int size = event.getTabs().size();
        final String eventString =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> {
                            return mActivity
                                    .getResources()
                                    .getString(R.string.recent_tabs_bulk_closure, size);
                        });
        List<String> domainList = new ArrayList<>();
        for (RecentlyClosedTab tab : event.getTabs()) {
            String domain = UrlUtilities.getDomainAndRegistry(tab.getUrl().getSpec(), false);
            domainList.add(domain);
        }
        final String eventDomainString =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> {
                            return mActivity
                                    .getResources()
                                    .getQuantityString(
                                            R.plurals.recent_tabs_group_closure_domain_text,
                                            event.getTabs().size(),
                                            event.getTabs().size(),
                                            String.join(", ", domainList));
                        });
        final View view = waitForTabCountTitleView(eventString);
        final TextView domainView = (TextView) waitForView(eventDomainString);
        assertEquals(eventDomainString, domainView.getText());

        mRenderTestRule.render(mPage.getView(), "recently_closed_bulk_event");

        final int groupIdx = !DeviceFormFactor.isNonMultiDisplayContextOnTablet(mActivity) ? 0 : 1;
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mPage.onChildClick(null, null, groupIdx, 0, 0);
                });
        verify(mManager, times(1)).openRecentlyClosedEntry(mTabModel, event);

        // Clear the recently closed tabs with the context menu and confirm the view is gone.
        openContextMenuAndInvokeItem(
                mActivity, view, RecentTabsRowAdapter.RecentlyClosedTabsGroup.ID_REMOVE_ALL);
        assertEquals(0, mManager.getRecentlyClosedEntries(1).size());
        waitForViewToDisappear(eventString);
    }

    @Test
    @MediumTest
    @Feature({"RecentTabsPage"})
    @DisableFeatures(ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS)
    public void testEmptyStateView_replaceSyncWithSignInDisabled() {
        // Sign in and enable sync.
        mSigninTestRule.addAccount(AccountManagerTestRule.TEST_ACCOUNT_NON_DISPLAYABLE_EMAIL);
        mSigninTestRule.waitForSignin(AccountManagerTestRule.TEST_ACCOUNT_NON_DISPLAYABLE_EMAIL);
        SigninTestUtil.signinAndEnableSync(
                AccountManagerTestRule.TEST_ACCOUNT_NON_DISPLAYABLE_EMAIL,
                SyncTestUtil.getSyncServiceForLastUsedProfile());

        // Open an empty recent tabs page and confirm empty view shows.
        mPage = loadRecentTabsPage();
        onView(
                        allOf(
                                withId(R.id.empty_state_container),
                                withParent(withId(R.id.legacy_sync_promo_view_frame_layout))))
                .check(matches(isDisplayed()));
    }

    @Test
    @MediumTest
    @Feature({"RecentTabsPage"})
    @EnableFeatures(ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS)
    public void testEmptyStateView() {
        mSigninTestRule.addAccount(AccountManagerTestRule.TEST_ACCOUNT_NON_DISPLAYABLE_EMAIL);
        SigninTestUtil.signinAndEnableHistorySync(
                AccountManagerTestRule.TEST_ACCOUNT_NON_DISPLAYABLE_EMAIL);

        // Open an empty recent tabs page and confirm empty view shows.
        mPage = loadRecentTabsPage();
        onView(
                        allOf(
                                withId(R.id.empty_state_container),
                                withParent(withId(R.id.legacy_sync_promo_view_frame_layout))))
                .check(matches(isDisplayed()));
    }

    @Test
    @SmallTest
    @DisableFeatures(ChromeFeatureList.TAB_STRIP_LAYOUT_OPTIMIZATION)
    public void testTabStripHeightChangeCallback() {
        mPage = loadRecentTabsPage();
        var tabStripHeightChangeCallback = mPage.getTabStripHeightChangeCallbackForTesting();
        int newTabStripHeight = 40;
        ThreadUtils.runOnUiThreadBlocking(
                () -> tabStripHeightChangeCallback.onResult(newTabStripHeight));
        assertEquals(
                "Top padding of page view should be updated when tab strip height changes.",
                newTabStripHeight,
                mPage.getView().getPaddingTop());
    }

    /**
     * Generates the specified number of {@link RecentlyClosedTab} instances and sets them on the
     * manager.
     */
    private void setRecentlyClosedEntries(List<RecentlyClosedEntry> entries) {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mManager.setRecentlyClosedEntries(entries);
                });
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
        CriteriaHelper.pollUiThread(
                () -> {
                    Criteria.checkThat(
                            "RecentTabsPage is still there",
                            mTab.getNativePage(),
                            Matchers.not(Matchers.instanceOf(RecentTabsPage.class)));
                });
    }

    /** Waits for the view with the specified text to appear. */
    private View waitForView(final String text) {
        final ArrayList<View> views = new ArrayList<>();
        CriteriaHelper.pollUiThread(
                () -> {
                    mPage.getView().findViewsWithText(views, text, View.FIND_VIEWS_WITH_TEXT);
                    Criteria.checkThat(
                            "Could not find view with this text: " + text,
                            views.size(),
                            Matchers.is(1));
                });
        return views.get(0);
    }

    /** Waits for the view with the specified text to appear. */
    private View waitForTabCountTitleView(final String text) {
        final ArrayList<View> views = new ArrayList<>();
        CriteriaHelper.pollUiThread(
                () -> {
                    mPage.getView().findViewsWithText(views, text, View.FIND_VIEWS_WITH_TEXT);
                    Criteria.checkThat(
                            "Could not find views with this text: " + text,
                            views.size(),
                            Matchers.is(2));
                });
        return views.get(0);
    }

    /** Waits for the view with the specified text to disappear. */
    private void waitForViewToDisappear(final String text) {
        CriteriaHelper.pollUiThread(
                () -> {
                    ArrayList<View> views = new ArrayList<>();
                    mPage.getView().findViewsWithText(views, text, View.FIND_VIEWS_WITH_TEXT);
                    Criteria.checkThat(
                            "View with this text is still present: " + text,
                            views,
                            Matchers.empty());
                });
    }

    private static void openContextMenuAndInvokeItem(
            final Activity activity, final View view, final int itemId) {
        // IMPLEMENTATION NOTE: Instrumentation.invokeContextMenuAction would've been much simpler,
        // but it requires the View to be focused which is hard to achieve in touch mode.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    view.performLongClick();
                    activity.getWindow().performContextMenuIdentifierAction(itemId, 0);
                });
    }
}
