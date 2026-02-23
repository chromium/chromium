// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.overlays.strip;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.graphics.Rect;
import android.view.View;
import android.widget.ListView;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.multiwindow.MultiWindowUtils;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.tabmodel.TabModel.RecentlyClosedEntryType;
import org.chromium.chrome.tab_ui.R;
import org.chromium.ui.listmenu.ListMenuItemProperties;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.ModelListAdapter;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.widget.AnchoredPopupWindow;
import org.chromium.ui.widget.RectProvider;

/** Unit tests for {@link TabStripContextMenuCoordinator}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@EnableFeatures({
    ChromeFeatureList.ROBUST_WINDOW_MANAGEMENT,
    ChromeFeatureList.TAB_STRIP_EMPTY_SPACE_CONTEXT_MENU_ANDROID
})
@DisableFeatures(ChromeFeatureList.GLIC)
public class TabStripContextMenuCoordinatorUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private TabStripContextMenuDelegate mDelegate;
    @Mock private RectProvider mRectProvider;

    private Activity mActivity;
    private TabStripContextMenuCoordinator mCoordinator;
    private AnchoredPopupWindow mMenuWindow;
    private View mContentView;
    private ListView mListView;

    @Before
    public void setUp() {
        mActivity = Robolectric.buildActivity(Activity.class).setup().get();
        mActivity.setTheme(R.style.Theme_BrowserUI_DayNight);
        mCoordinator = new TabStripContextMenuCoordinator(mActivity, mDelegate);
        when(mRectProvider.getRect())
                .thenReturn(new Rect(10, 10, mActivity.getWindow().getDecorView().getWidth(), 50));
        when(mDelegate.getRecentlyClosedEntryType()).thenReturn(RecentlyClosedEntryType.TAB);
        when(mDelegate.getTabCount()).thenReturn(2);
    }

    @Test
    @DisableFeatures({
        ChromeFeatureList.ROBUST_WINDOW_MANAGEMENT,
        ChromeFeatureList.TAB_STRIP_EMPTY_SPACE_CONTEXT_MENU_ANDROID
    })
    public void showMenu_emptyList_verifyMenuState() {
        // Arrange.
        MultiWindowUtils.setMultiInstanceApi31EnabledForTesting(true);

        // Act.
        mCoordinator.showMenu(mRectProvider, false, mActivity);

        // Verify.
        verifyMenuState(/* expectedNumItems= */ 0);
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
        when(mDelegate.getRecentlyClosedEntryType()).thenReturn(RecentlyClosedEntryType.NONE);

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
        verify(mDelegate).onNewTab();
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
        verify(mDelegate).onReopenClosedEntry();
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
        verify(mDelegate).onBookmarkAllTabs();
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
        verify(mDelegate).onNameWindow();
        assertFalse(mMenuWindow.isShowing());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.GLIC)
    public void showMenu_verifyPinGlicOption() {
        // Arrange.
        MultiWindowUtils.setMultiInstanceApi31EnabledForTesting(true);
        ChromeSharedPreferences.getInstance()
                .writeBoolean(ChromePreferenceKeys.GLIC_BUTTON_PINNED, false);
        mCoordinator.showMenu(mRectProvider, false, mActivity);
        verifyMenuState(/* expectedNumItems= */ 6);
        assertEquals(
                R.string.menu_pin_glic,
                getItemModelAtPosition(5).get(ListMenuItemProperties.TITLE_ID));

        // Act: Select "Pin Gemini" option.
        mCoordinator
                .getListMenuDelegate(mContentView)
                .onItemSelected(getItemModelAtPosition(5), mListView);

        // Verify.
        verify(mDelegate).onPinGlic();
        assertFalse(mMenuWindow.isShowing());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.GLIC)
    public void showMenu_verifyUnpinGlicOption() {
        // Arrange.
        MultiWindowUtils.setMultiInstanceApi31EnabledForTesting(true);
        ChromeSharedPreferences.getInstance()
                .writeBoolean(ChromePreferenceKeys.GLIC_BUTTON_PINNED, true);
        mCoordinator.showMenu(mRectProvider, false, mActivity);
        verifyMenuState(/* expectedNumItems= */ 6);
        assertEquals(
                R.string.menu_unpin_glic,
                getItemModelAtPosition(5).get(ListMenuItemProperties.TITLE_ID));

        // Act: Select "Unpin Gemini" option.
        mCoordinator
                .getListMenuDelegate(mContentView)
                .onItemSelected(getItemModelAtPosition(5), mListView);

        // Verify.
        verify(mDelegate).onUnpinGlic();
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

    private PropertyModel getItemModelAtPosition(int position) {
        var adapter = (ModelListAdapter) mListView.getAdapter();
        return ((ListItem) adapter.getItem(position)).model;
    }
}
