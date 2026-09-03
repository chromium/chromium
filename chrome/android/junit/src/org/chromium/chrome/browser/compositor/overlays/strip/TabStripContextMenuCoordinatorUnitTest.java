// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.overlays.strip;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.never;
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
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
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
import org.chromium.chrome.browser.ui.messages.snackbar.Snackbar;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.browser.ui.vertical_tabs.VerticalTabUtils;
import org.chromium.chrome.tab_ui.R;
import org.chromium.components.feature_engagement.FeatureConstants;
import org.chromium.components.feature_engagement.Tracker;
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
    @Mock private Tracker mTracker;
    @Mock private PrefService mPrefService;
    @Mock private UserPrefs.Natives mUserPrefsJniMock;
    @Mock private TaskManager mTaskManager;

    private Activity mActivity;
    private TabStripContextMenuCoordinator mCoordinator;
    private AnchoredPopupWindow mMenuWindow;
    private View mContentView;
    private ListView mListView;

    @Before
    public void setUp() {
        TrackerFactory.setTrackerForTests(mTracker);
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
        DeviceInfo.resetIsDesktopForTesting();
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

        // Verify: Baseline items (4) + divider (1) + toggle item (1) = 6 items.
        verifyMenuState(/* expectedNumItems= */ 6);

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
        verifyMenuState(/* expectedNumItems= */ 6);

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
        when(mTracker.shouldTriggerHelpUi(FeatureConstants.ANDROID_VERTICAL_TABS_NEW_LABEL))
                .thenReturn(true);

        mCoordinator.showMenu(mRectProvider, false, mActivity);

        verifyMenuState(/* expectedNumItems= */ 6);

        PropertyModel toggleLayoutItemModel = getItemModelAtPosition(5);
        CharSequence title = toggleLayoutItemModel.get(ListMenuItemProperties.TITLE);
        assertNotNull(title);
        assertTrue(title.toString().contains(mActivity.getString(R.string.show_tabs_vertically)));

        // Verify the "New" badge spans are included.
        assertTrue("Title should be a Spannable carrying badge spans.", title instanceof Spannable);
        Spannable spannableTitle = (Spannable) title;
        Object[] spans = spannableTitle.getSpans(0, spannableTitle.length(), Object.class);
        assertTrue("Spannable title should contain badge styling spans.", spans.length > 0);

        verify(mTracker).shouldTriggerHelpUi(FeatureConstants.ANDROID_VERTICAL_TABS_NEW_LABEL);
        verify(mTracker).dismissed(FeatureConstants.ANDROID_VERTICAL_TABS_NEW_LABEL);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_VERTICAL_TABS)
    @Config(qualifiers = "sw600dp")
    public void showMenu_suppressesBadgeWhenTrackerReturnsFalse() {
        MultiWindowUtils.setMultiInstanceApi31EnabledForTesting(true);
        VerticalTabUtils.setVerticalTabsEnabled(false);
        when(mTracker.shouldTriggerHelpUi(FeatureConstants.ANDROID_VERTICAL_TABS_NEW_LABEL))
                .thenReturn(false);

        mCoordinator.showMenu(mRectProvider, false, mActivity);

        verifyMenuState(/* expectedNumItems= */ 6);

        PropertyModel toggleLayoutItemModel = getItemModelAtPosition(5);
        CharSequence title = toggleLayoutItemModel.get(ListMenuItemProperties.TITLE);
        assertNotNull(title);
        assertTrue(title.toString().contains(mActivity.getString(R.string.show_tabs_vertically)));
        assertFalse("Title should not contain New text", title.toString().contains("New"));
        assertFalse("Title should not carry badge spans", title instanceof Spannable);
        verify(mTracker).shouldTriggerHelpUi(FeatureConstants.ANDROID_VERTICAL_TABS_NEW_LABEL);
        verify(mTracker, never()).dismissed(any());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_VERTICAL_TABS)
    @Config(qualifiers = "sw600dp")
    public void showMenu_desktopDevice_suppressesNewBadge() {
        MultiWindowUtils.setMultiInstanceApi31EnabledForTesting(true);
        VerticalTabUtils.setVerticalTabsEnabled(false);

        // Mock device form factor as Desktop.
        DeviceInfo.setIsDesktopForTesting(true);

        mCoordinator.showMenu(mRectProvider, false, mActivity);
        verifyMenuState(/* expectedNumItems= */ 8);

        PropertyModel toggleLayoutItemModel = null;
        for (int i = 0; i < mListView.getAdapter().getCount(); i++) {
            ListItem item = (ListItem) mListView.getAdapter().getItem(i);
            if (item.model.containsKey(ListMenuItemProperties.MENU_ITEM_ID)
                    && item.model.get(ListMenuItemProperties.MENU_ITEM_ID)
                            == R.id.toggle_tab_layout_menu_id) {
                toggleLayoutItemModel = item.model;
                break;
            }
        }
        assertNotNull(toggleLayoutItemModel);
        CharSequence title = toggleLayoutItemModel.get(ListMenuItemProperties.TITLE);
        assertNotNull(title);
        assertTrue(title.toString().contains(mActivity.getString(R.string.show_tabs_vertically)));
        assertFalse("Title should not carry badge spans on desktop", title instanceof Spannable);
        verify(mTracker, never()).shouldTriggerHelpUi(any());
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

        // Verify: Expected items: New tab, Reopen closed tab (disabled), Name window.
        verifyMenuState(/* expectedNumItems= */ 3);
        assertEquals(
                R.string.menu_new_tab,
                getItemModelAtPosition(0).get(ListMenuItemProperties.TITLE_ID));
        assertEquals(
                R.string.menu_reopen_closed_tab,
                getItemModelAtPosition(1).get(ListMenuItemProperties.TITLE_ID));
        assertFalse(getItemModelAtPosition(1).get(ListMenuItemProperties.ENABLED));
        assertEquals(
                R.string.menu_name_window,
                getItemModelAtPosition(2).get(ListMenuItemProperties.TITLE_ID));
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
        assertTrue(getItemModelAtPosition(1).get(ListMenuItemProperties.ENABLED));

        // Act: Select "Reopen closed tab" option.
        mCoordinator
                .getListMenuDelegate(mContentView)
                .onItemSelected(getItemModelAtPosition(1), mListView);

        // Verify.
        verify(mTabModel).openMostRecentlyClosedEntry();
        assertFalse(mMenuWindow.isShowing());
    }

    @Test
    public void showMenu_verifyReopenClosedEntryOption_Disabled() {
        // Arrange.
        MultiWindowUtils.setMultiInstanceApi31EnabledForTesting(true);
        when(mTabModel.getMostRecentlyClosedEntryType()).thenReturn(RecentlyClosedEntryType.NONE);

        // Act.
        mCoordinator.showMenu(mRectProvider, false, mActivity);

        // Verify.
        verifyMenuState(/* expectedNumItems= */ 4);
        assertEquals(
                R.string.menu_reopen_closed_tab,
                getItemModelAtPosition(1).get(ListMenuItemProperties.TITLE_ID));
        assertFalse(getItemModelAtPosition(1).get(ListMenuItemProperties.ENABLED));
    }

    @Test
    public void showMenu_verifyReopenClosedEntryOption_Tabs() {
        // Arrange.
        MultiWindowUtils.setMultiInstanceApi31EnabledForTesting(true);
        when(mTabModel.getMostRecentlyClosedEntryType()).thenReturn(RecentlyClosedEntryType.TABS);

        // Act.
        mCoordinator.showMenu(mRectProvider, false, mActivity);

        // Verify.
        verifyMenuState(/* expectedNumItems= */ 4);
        assertEquals(
                R.string.menu_reopen_closed_tabs,
                getItemModelAtPosition(1).get(ListMenuItemProperties.TITLE_ID));
        assertTrue(getItemModelAtPosition(1).get(ListMenuItemProperties.ENABLED));
    }

    @Test
    public void showMenu_verifyReopenClosedEntryOption_Group() {
        // Arrange.
        MultiWindowUtils.setMultiInstanceApi31EnabledForTesting(true);
        when(mTabModel.getMostRecentlyClosedEntryType()).thenReturn(RecentlyClosedEntryType.GROUP);

        // Act.
        mCoordinator.showMenu(mRectProvider, false, mActivity);

        // Verify.
        verifyMenuState(/* expectedNumItems= */ 4);
        assertEquals(
                R.string.menu_reopen_closed_group,
                getItemModelAtPosition(1).get(ListMenuItemProperties.TITLE_ID));
        assertTrue(getItemModelAtPosition(1).get(ListMenuItemProperties.ENABLED));
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
        assertTrue(getItemModelAtPosition(2).get(ListMenuItemProperties.ENABLED));

        // Act: Select "Bookmark all tabs" option.
        mCoordinator
                .getListMenuDelegate(mContentView)
                .onItemSelected(getItemModelAtPosition(2), mListView);

        // Verify.
        assertFalse(mMenuWindow.isShowing());
    }

    @Test
    public void showMenu_verifyBookmarkAllTabs_Disabled() {
        // Arrange.
        MultiWindowUtils.setMultiInstanceApi31EnabledForTesting(true);
        when(mTabModel.getCount()).thenReturn(1);

        // Act.
        mCoordinator.showMenu(mRectProvider, false, mActivity);

        // Verify.
        verifyMenuState(/* expectedNumItems= */ 4);
        assertEquals(
                R.string.menu_bookmark_all_tabs,
                getItemModelAtPosition(2).get(ListMenuItemProperties.TITLE_ID));
        assertFalse(getItemModelAtPosition(2).get(ListMenuItemProperties.ENABLED));
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
    @EnableFeatures({ChromeFeatureList.GLIC, ChromeFeatureList.ENABLE_ANDROID_SIDE_PANEL})
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
    @EnableFeatures({ChromeFeatureList.GLIC, ChromeFeatureList.ENABLE_ANDROID_SIDE_PANEL})
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
        verify(mSnackbarManager).showSnackbar(any(Snackbar.class));
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
