// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.overlays.strip;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.graphics.Rect;
import android.text.Spannable;
import android.view.View;
import android.widget.ListView;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;

import org.chromium.base.ContextUtils;
import org.chromium.base.DeviceInfo;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.compositor.overlays.strip.TabContextMenuCoordinator.TabStripLayoutType;
import org.chromium.chrome.browser.feedback.FeedbackPolicyManager;
import org.chromium.chrome.browser.feedback.HelpAndFeedbackLauncher;
import org.chromium.chrome.browser.feedback.HelpAndFeedbackLauncherFactory;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.glic.GlicEnabling;
import org.chromium.chrome.browser.glic.GlicPrefNames;
import org.chromium.chrome.browser.multiwindow.MultiInstanceManager;
import org.chromium.chrome.browser.multiwindow.MultiWindowUtils;
import org.chromium.chrome.browser.multiwindow.UiUtils.NameWindowDialogSource;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tabmodel.TabList;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModel.RecentlyClosedEntryType;
import org.chromium.chrome.browser.task_manager.TaskManager;
import org.chromium.chrome.browser.task_manager.TaskManagerFactory;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.browser.ui.vertical_tabs.VerticalTabUtils;
import org.chromium.chrome.tab_ui.R;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.components.user_prefs.UserPrefsJni;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.listmenu.ListItemType;
import org.chromium.ui.listmenu.ListMenuItemProperties;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.ModelListAdapter;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.widget.AnchoredPopupWindow;
import org.chromium.ui.widget.RectProvider;

import java.lang.ref.WeakReference;
import java.util.Collections;

/** Unit tests for {@link TabStripContextMenuCoordinator}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@DisableFeatures({
    ChromeFeatureList.GLIC,
    ChromeFeatureList.TASK_MANAGER_CLANK,
    ChromeFeatureList.DARKEN_WEBSITES_CHECKBOX_IN_THEMES_SETTING
})
public class TabStripContextMenuCoordinatorUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private TabModel mTabModel;
    @Mock private MultiInstanceManager mMultiInstanceManager;
    @Mock private WindowAndroid mWindowAndroid;
    @Mock private SnackbarManager mSnackbarManager;
    @Mock private Runnable mOnNewTabClick;
    @Mock private RectProvider mRectProvider;
    @Mock private Profile mProfile;
    @Mock private PrefService mPrefService;
    @Mock private UserPrefs.Natives mUserPrefsJniMock;
    @Mock private TaskManager mTaskManager;
    @Mock private HelpAndFeedbackLauncher mHelpAndFeedbackLauncher;
    @Mock private FeedbackPolicyManager mFeedbackPolicyManager;

    private Activity mActivity;
    private TabStripContextMenuCoordinator mCoordinator;
    private AnchoredPopupWindow mMenuWindow;
    private View mContentView;
    private ListView mListView;

    @Before
    public void setUp() {
        GlicEnabling.setEnabledForTesting(ChromeFeatureList.isEnabled(ChromeFeatureList.GLIC));
        mActivity = Robolectric.buildActivity(Activity.class).setup().get();
        mActivity.setTheme(R.style.Theme_BrowserUI_DayNight);

        when(mWindowAndroid.getActivity()).thenReturn(new WeakReference<>(mActivity));
        when(mTabModel.getMostRecentlyClosedEntryType()).thenReturn(RecentlyClosedEntryType.TAB);
        when(mTabModel.getCount()).thenReturn(2);
        when(mTabModel.getProfile()).thenReturn(mProfile);
        when(mProfile.getOriginalProfile()).thenReturn(mProfile);

        doAnswer(invocation -> Collections.emptyIterator()).when((TabList) mTabModel).iterator();

        initializeCoordinatorForTesting(TabStripLayoutType.HORIZONTAL);

        UserPrefsJni.setInstanceForTesting(mUserPrefsJniMock);
        when(mUserPrefsJniMock.get(mProfile)).thenReturn(mPrefService);
        TaskManagerFactory.setInstanceForTesting(mTaskManager);
        HelpAndFeedbackLauncherFactory.setInstanceForTesting(mHelpAndFeedbackLauncher);
        FeedbackPolicyManager.setInstanceForTesting(mFeedbackPolicyManager);
        when(mFeedbackPolicyManager.isUserFeedbackAllowed()).thenReturn(true);

        when(mRectProvider.getRect())
                .thenReturn(new Rect(10, 10, mActivity.getWindow().getDecorView().getWidth(), 50));
    }

    private void initializeCoordinatorForTesting(@TabStripLayoutType int layout) {
        mCoordinator =
                TabStripContextMenuCoordinator.createContextMenuCoordinator(
                        mTabModel,
                        mMultiInstanceManager,
                        mWindowAndroid,
                        mSnackbarManager,
                        mOnNewTabClick,
                        /* canActivateTabLayoutToggleMenuSupplier= */ null,
                        layout);
    }

    @After
    public void tearDown() {
        ChromeSharedPreferences.getInstance().removeKey(ChromePreferenceKeys.VERTICAL_TABS_ENABLED);
        ChromeSharedPreferences.getInstance()
                .removeKey(ChromePreferenceKeys.VERTICAL_TABS_LAYOUT_TOGGLE_VIEW_COUNT);
    }

    private void runToggleLayoutMenuTest(boolean isVerticalTabsEnabled, int expectedTitleRes) {
        MultiWindowUtils.setMultiInstanceApi31EnabledForTesting(true);
        VerticalTabUtils.setVerticalTabsEnabled(isVerticalTabsEnabled);
        initializeCoordinatorForTesting(
                isVerticalTabsEnabled
                        ? TabStripLayoutType.VERTICAL
                        : TabStripLayoutType.HORIZONTAL);

        // Act.
        mCoordinator.showMenu(mRectProvider, false, mActivity);

        // Verify: Baseline items (4) + divider (1) + toggle item (1) + feedback (1) = 7 items.
        verifyMenuState(/* expectedNumItems= */ 7);

        // Index 4 is the divider.
        ListItem dividerItem = (ListItem) mListView.getAdapter().getItem(4);
        assertEquals(ListItemType.DIVIDER, dividerItem.type);

        // Index 5 is the layout toggle entry point.
        PropertyModel toggleLayoutItemModel = getItemModelAtPosition(5);
        assertEquals(
                R.id.toggle_tab_layout_menu_id,
                toggleLayoutItemModel.get(ListMenuItemProperties.MENU_ITEM_ID));
        // Check if the item sets TITLE directly as CharSequence/String or badged ("New")
        // CharSequence.
        CharSequence actualTitle = toggleLayoutItemModel.get(ListMenuItemProperties.TITLE);
        assertNotNull(actualTitle);
        assertTrue(actualTitle.toString().contains(mActivity.getString(expectedTitleRes)));

        // Index 6 is the feedback entry point.
        PropertyModel feedbackItemModel = getItemModelAtPosition(6);
        assertEquals(
                R.id.send_feedback_about_tab_strip_menu_id,
                feedbackItemModel.get(ListMenuItemProperties.MENU_ITEM_ID));
        assertEquals(
                R.string.send_feedback_about_tab_strip,
                feedbackItemModel.get(ListMenuItemProperties.TITLE_ID));

        // Act: Select the toggle option.
        mCoordinator
                .getListMenuDelegate(mContentView)
                .onItemSelected(toggleLayoutItemModel, mListView);

        assertFalse(mMenuWindow.isShowing());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_VERTICAL_TABS)
    @Config(qualifiers = "sw600dp")
    public void showMenu_verifyVerticalTabsEntryPoint() {
        runToggleLayoutMenuTest(/* isVerticalTabsEnabled= */ false, R.string.show_tabs_vertically);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_VERTICAL_TABS)
    @Config(qualifiers = "sw600dp")
    public void showMenu_verifyHorizontalTabsEntryPoint() {
        runToggleLayoutMenuTest(/* isVerticalTabsEnabled= */ true, R.string.show_tabs_horizontally);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_VERTICAL_TABS)
    @Config(qualifiers = "sw600dp")
    public void showMenu_verifyVerticalTabsDisabledWhenCannotActivate() {
        mCoordinator =
                TabStripContextMenuCoordinator.createContextMenuCoordinator(
                        mTabModel,
                        mMultiInstanceManager,
                        mWindowAndroid,
                        mSnackbarManager,
                        mOnNewTabClick,
                        () -> false,
                        TabStripLayoutType.HORIZONTAL);
        MultiWindowUtils.setMultiInstanceApi31EnabledForTesting(true);

        mCoordinator.showMenu(mRectProvider, false, mActivity);
        verifyMenuState(/* expectedNumItems= */ 7);

        PropertyModel toggleLayoutItemModel = getItemModelAtPosition(5);
        assertEquals(
                R.id.toggle_tab_layout_menu_id,
                toggleLayoutItemModel.get(ListMenuItemProperties.MENU_ITEM_ID));
        assertFalse(toggleLayoutItemModel.get(ListMenuItemProperties.ENABLED));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_VERTICAL_TABS)
    @Config(qualifiers = "sw600dp")
    public void showMenu_verifyVerticalTabsEntryPoint_showsNewBadge() {
        MultiWindowUtils.setMultiInstanceApi31EnabledForTesting(true);
        VerticalTabUtils.setVerticalTabsEnabled(false);
        ChromeSharedPreferences.getInstance()
                .writeInt(ChromePreferenceKeys.VERTICAL_TABS_LAYOUT_TOGGLE_VIEW_COUNT, 0);

        mCoordinator.showMenu(mRectProvider, false, mActivity);

        verifyMenuState(/* expectedNumItems= */ 7);

        PropertyModel toggleLayoutItemModel = getItemModelAtPosition(5);
        CharSequence title = toggleLayoutItemModel.get(ListMenuItemProperties.TITLE);
        assertNotNull(title);
        assertTrue(title.toString().contains(mActivity.getString(R.string.show_tabs_vertically)));

        // Verify the "New" badge spans are included.
        assertTrue("Title should be a Spannable carrying badge spans.", title instanceof Spannable);
        Spannable spannableTitle = (Spannable) title;
        Object[] spans = spannableTitle.getSpans(0, spannableTitle.length(), Object.class);
        assertTrue("Spannable title should contain badge styling spans.", spans.length > 0);

        // Verify view count incremented from 0 to 1 upon showing.
        assertEquals(1, VerticalTabUtils.getNewBadgeViewCount());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_VERTICAL_TABS)
    @Config(qualifiers = "sw600dp")
    public void showMenu_clickVerticalTabsEntryPoint_dismissesBadge() {
        MultiWindowUtils.setMultiInstanceApi31EnabledForTesting(true);
        // Start with the horizontal tab.
        VerticalTabUtils.setVerticalTabsEnabled(false);
        ChromeSharedPreferences.getInstance()
                .writeInt(ChromePreferenceKeys.VERTICAL_TABS_LAYOUT_TOGGLE_VIEW_COUNT, 0);

        mCoordinator.showMenu(mRectProvider, false, mActivity);

        verifyMenuState(7);

        PropertyModel toggleLayoutItemModel = getItemModelAtPosition(5);

        // Act: Select the toggle option.
        mCoordinator
                .getListMenuDelegate(mContentView)
                .onItemSelected(toggleLayoutItemModel, mListView);

        // Simulate enabling vertical tabs as a result of the user selection.
        VerticalTabUtils.setVerticalTabsEnabled(true);

        // Verify view count was set to Max count (3), permanently suppressing the badge.
        assertEquals(
                VerticalTabUtils.NEW_BADGE_MAX_VIEW_COUNT, VerticalTabUtils.getNewBadgeViewCount());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_VERTICAL_TABS)
    @Config(qualifiers = "sw600dp")
    public void showMenu_desktopDevice_suppressesNewBadge() {
        MultiWindowUtils.setMultiInstanceApi31EnabledForTesting(true);
        VerticalTabUtils.setVerticalTabsEnabled(false);
        ChromeSharedPreferences.getInstance()
                .writeInt(ChromePreferenceKeys.VERTICAL_TABS_LAYOUT_TOGGLE_VIEW_COUNT, 0);

        // Mock device form factor as Desktop.
        DeviceInfo.setIsDesktopForTesting(true);

        mCoordinator.showMenu(mRectProvider, false, mActivity);
        verifyMenuState(/* expectedNumItems= */ 9);

        // View count should remain 0 because Desktop suppresses the badge. This feature is only for
        // tablets.
        assertEquals(0, VerticalTabUtils.getNewBadgeViewCount());
    }

    @Test
    public void showMenu_verifyMenuState() {
        // Arrange.
        MultiWindowUtils.setMultiInstanceApi31EnabledForTesting(true);

        // Act.
        mCoordinator.showMenu(mRectProvider, false, mActivity);

        // Verify.
        verifyMenuState(/* expectedNumItems= */ 4);
    }

    @Test
    public void showMenu_verifyMenuState_Incognito() {
        // Arrange.
        MultiWindowUtils.setMultiInstanceApi31EnabledForTesting(true);
        // In Incognito, there are no recently closed entries.
        when(mTabModel.getMostRecentlyClosedEntryType()).thenReturn(RecentlyClosedEntryType.NONE);

        // Act.
        mCoordinator.showMenu(mRectProvider, true, mActivity);

        // Verify: Expected items: New tab, Name window.
        verifyMenuState(/* expectedNumItems= */ 2);
        assertEquals(
                R.string.menu_new_tab,
                getItemModelAtPosition(0).get(ListMenuItemProperties.TITLE_ID));
        assertEquals(
                R.string.menu_name_window,
                getItemModelAtPosition(1).get(ListMenuItemProperties.TITLE_ID));
    }

    @Test
    public void showMenu_verifyMenuState_noMultiInstanceSupport() {
        // Arrange.
        MultiWindowUtils.setMultiInstanceApi31EnabledForTesting(false);

        // Act.
        mCoordinator.showMenu(mRectProvider, false, mActivity);

        // Verify.
        verifyMenuState(/* expectedNumItems= */ 3);
    }

    @Test
    public void showMenu_verifyNewTabOption() {
        // Arrange.
        MultiWindowUtils.setMultiInstanceApi31EnabledForTesting(true);
        mCoordinator.showMenu(mRectProvider, false, mActivity);
        verifyMenuState(/* expectedNumItems= */ 4);
        assertEquals(
                R.string.menu_new_tab,
                getItemModelAtPosition(0).get(ListMenuItemProperties.TITLE_ID));

        // Act: Select "New tab" option.
        mCoordinator
                .getListMenuDelegate(mContentView)
                .onItemSelected(getItemModelAtPosition(0), mListView);

        // Verify.
        verify(mOnNewTabClick).run();
        assertFalse(mMenuWindow.isShowing());
    }

    @Test
    public void showMenu_verifyReopenClosedEntryOption() {
        // Arrange.
        MultiWindowUtils.setMultiInstanceApi31EnabledForTesting(true);
        mCoordinator.showMenu(mRectProvider, false, mActivity);
        verifyMenuState(/* expectedNumItems= */ 4);
        assertEquals(
                R.string.menu_reopen_closed_tab,
                getItemModelAtPosition(1).get(ListMenuItemProperties.TITLE_ID));

        // Act: Select "Reopen closed tab" option.
        mCoordinator
                .getListMenuDelegate(mContentView)
                .onItemSelected(getItemModelAtPosition(1), mListView);

        // Verify.
        verify(mTabModel).openMostRecentlyClosedEntry();
        assertFalse(mMenuWindow.isShowing());
    }

    @Test
    public void showMenu_verifyBookmarkAllTabs() {
        // Arrange.
        MultiWindowUtils.setMultiInstanceApi31EnabledForTesting(true);
        mCoordinator.showMenu(mRectProvider, false, mActivity);
        verifyMenuState(/* expectedNumItems= */ 4);
        assertEquals(
                R.string.menu_bookmark_all_tabs,
                getItemModelAtPosition(2).get(ListMenuItemProperties.TITLE_ID));

        // Act: Select "Bookmark all tabs" option.
        mCoordinator
                .getListMenuDelegate(mContentView)
                .onItemSelected(getItemModelAtPosition(2), mListView);

        // Verify.
        assertFalse(mMenuWindow.isShowing());
    }

    @Test
    public void showMenu_verifyNameWindowOption() {
        // Arrange.
        MultiWindowUtils.setMultiInstanceApi31EnabledForTesting(true);
        mCoordinator.showMenu(mRectProvider, false, mActivity);
        verifyMenuState(/* expectedNumItems= */ 4);
        assertEquals(
                R.string.menu_name_window,
                getItemModelAtPosition(3).get(ListMenuItemProperties.TITLE_ID));

        // Act: Select "Name window" option.
        mCoordinator
                .getListMenuDelegate(mContentView)
                .onItemSelected(getItemModelAtPosition(3), mListView);

        // Verify.
        verify(mMultiInstanceManager).showNameWindowDialog(NameWindowDialogSource.TAB_STRIP);
        assertFalse(mMenuWindow.isShowing());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.GLIC)
    public void showMenu_verifyPinGlicOption() {
        // Arrange.
        MultiWindowUtils.setMultiInstanceApi31EnabledForTesting(true);
        when(mPrefService.getBoolean(GlicPrefNames.GLIC_PINNED_TO_TABSTRIP)).thenReturn(false);
        mCoordinator.showMenu(mRectProvider, false, mActivity);
        verifyMenuState(/* expectedNumItems= */ 6);
        assertEquals(
                R.string.glic_pin, getItemModelAtPosition(5).get(ListMenuItemProperties.TITLE_ID));

        // Act: Select "Pin Gemini" option.
        mCoordinator
                .getListMenuDelegate(mContentView)
                .onItemSelected(getItemModelAtPosition(5), mListView);

        // Verify.
        verify(mPrefService).setBoolean(GlicPrefNames.GLIC_PINNED_TO_TABSTRIP, true);
        assertFalse(mMenuWindow.isShowing());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.GLIC)
    public void showMenu_verifyUnpinGlicOption() {
        // Arrange.
        MultiWindowUtils.setMultiInstanceApi31EnabledForTesting(true);
        when(mPrefService.getBoolean(GlicPrefNames.GLIC_PINNED_TO_TABSTRIP)).thenReturn(true);
        mCoordinator.showMenu(mRectProvider, false, mActivity);
        verifyMenuState(/* expectedNumItems= */ 6);
        assertEquals(
                R.string.glic_unpin,
                getItemModelAtPosition(5).get(ListMenuItemProperties.TITLE_ID));

        // Act: Select "Unpin Gemini" option.
        mCoordinator
                .getListMenuDelegate(mContentView)
                .onItemSelected(getItemModelAtPosition(5), mListView);

        // Verify.
        verify(mPrefService).setBoolean(GlicPrefNames.GLIC_PINNED_TO_TABSTRIP, false);
        assertFalse(mMenuWindow.isShowing());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.TASK_MANAGER_CLANK)
    public void showMenu_verifyTaskManagerOption() {
        // Arrange.
        MultiWindowUtils.setMultiInstanceApi31EnabledForTesting(true);

        // Act.
        mCoordinator.showMenu(mRectProvider, false, mActivity);

        // Verify: Baseline (4) + Divider (1) + Task Manager (1) = 6 items.
        verifyMenuState(/* expectedNumItems= */ 6);

        // Index 4 is the divider.
        ListItem dividerItem = (ListItem) mListView.getAdapter().getItem(4);
        assertEquals(ListItemType.DIVIDER, dividerItem.type);

        // Index 5 is the Task Manager.
        PropertyModel taskManagerItemModel = getItemModelAtPosition(5);
        assertEquals(
                R.id.task_manager, taskManagerItemModel.get(ListMenuItemProperties.MENU_ITEM_ID));
        assertEquals(
                R.string.menu_task_manager,
                taskManagerItemModel.get(ListMenuItemProperties.TITLE_ID));

        // Act: Select the Task Manager option.
        mCoordinator
                .getListMenuDelegate(mContentView)
                .onItemSelected(taskManagerItemModel, mListView);

        // Verify.
        verify(mTaskManager).launch(ContextUtils.getApplicationContext());
        assertFalse(mMenuWindow.isShowing());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_VERTICAL_TABS)
    @Config(qualifiers = "sw600dp")
    public void showMenu_verifySendFeedbackOption() {
        MultiWindowUtils.setMultiInstanceApi31EnabledForTesting(true);
        mCoordinator.showMenu(mRectProvider, false, mActivity);

        // Verify: expected 7 items.
        verifyMenuState(/* expectedNumItems= */ 7);

        // Index 6 is feedback option.
        PropertyModel feedbackItemModel = getItemModelAtPosition(6);
        assertEquals(
                R.id.send_feedback_about_tab_strip_menu_id,
                feedbackItemModel.get(ListMenuItemProperties.MENU_ITEM_ID));

        // Act: Click the feedback option.
        mCoordinator.getListMenuDelegate(mContentView).onItemSelected(feedbackItemModel, mListView);

        // Verify: The popup window was dismissed and showFeedback was called with category tag.
        verify(mHelpAndFeedbackLauncher)
                .showFeedback(eq(mActivity), eq(null), eq(mCoordinator.getFeedbackCategoryTag()));
        assertFalse(mMenuWindow.isShowing());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_VERTICAL_TABS)
    @Config(qualifiers = "sw600dp")
    public void showMenu_verifySendFeedbackOption_Incognito() {
        MultiWindowUtils.setMultiInstanceApi31EnabledForTesting(true);
        // Setup Incognito profile and tab model.
        Profile incognitoProfile = mock(Profile.class);
        when(incognitoProfile.isOffTheRecord()).thenReturn(true);
        when(incognitoProfile.getOriginalProfile()).thenReturn(mProfile);
        when(mTabModel.getProfile()).thenReturn(incognitoProfile);
        when(mTabModel.getMostRecentlyClosedEntryType()).thenReturn(RecentlyClosedEntryType.NONE);

        // Act.
        mCoordinator.showMenu(mRectProvider, true, mActivity);

        // Verify: Expected items: New tab, Name window, divider, layout option, Send feedback.
        verifyMenuState(/* expectedNumItems= */ 5);

        // Index 4 is feedback option.
        PropertyModel feedbackItemModel = getItemModelAtPosition(4);
        assertEquals(
                R.id.send_feedback_about_tab_strip_menu_id,
                feedbackItemModel.get(ListMenuItemProperties.MENU_ITEM_ID));

        // Act: Click the feedback option.
        mCoordinator.getListMenuDelegate(mContentView).onItemSelected(feedbackItemModel, mListView);

        // Verify: The popup window was dismissed and showFeedback was called with a null URL and
        // category tag.
        verify(mHelpAndFeedbackLauncher)
                .showFeedback(eq(mActivity), eq(null), eq(mCoordinator.getFeedbackCategoryTag()));
        assertFalse(mMenuWindow.isShowing());
    }

    private void verifyMenuState(int expectedNumItems) {
        mMenuWindow = mCoordinator.getPopupWindow();
        if (expectedNumItems > 0) {
            assertNotNull(mMenuWindow);
            assertTrue(mMenuWindow.isShowing());
            mContentView = mMenuWindow.getContentView();
            mListView = mContentView.findViewById(R.id.tab_group_action_menu_list);
            var adapter = (ModelListAdapter) mListView.getAdapter();
            assertEquals(expectedNumItems, adapter.getCount());
        } else {
            assertNull(mMenuWindow);
        }
    }

    @Test
    public void showMenu_verifyMenuWidthSizing() {
        // Set a small anchor rect width (100px, which is less than min_width).
        when(mRectProvider.getRect()).thenReturn(new Rect(10, 10, 110, 50));

        // Act.
        mCoordinator.showMenu(mRectProvider, false, mActivity);

        // Verify popup window is created and showing.
        assertNotNull(mCoordinator.getPopupWindow());
        assertTrue(mCoordinator.getPopupWindow().isShowing());

        // Verify that the content view is measured at or above min_width, rather than being
        // restricted to 100px.
        View contentView = mCoordinator.getPopupWindow().getContentView();
        assertNotNull(contentView);
        assertTrue(
                "Content view width should be at least min_width.",
                contentView.getMeasuredWidth()
                        >= mActivity
                                .getResources()
                                .getDimensionPixelSize(R.dimen.tab_strip_context_menu_min_width));
    }

    private PropertyModel getItemModelAtPosition(int position) {
        var adapter = (ModelListAdapter) mListView.getAdapter();
        return ((ListItem) adapter.getItem(position)).model;
    }
}
