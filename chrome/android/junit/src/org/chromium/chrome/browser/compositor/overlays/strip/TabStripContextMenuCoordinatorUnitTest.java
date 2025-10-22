// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.overlays.strip;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
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
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.multiwindow.MultiInstanceManager;
import org.chromium.chrome.browser.multiwindow.MultiWindowUtils;
import org.chromium.chrome.browser.multiwindow.UiUtils.NameWindowDialogSource;
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
public class TabStripContextMenuCoordinatorUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private MultiInstanceManager mMultiInstanceManager;
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
        mCoordinator = new TabStripContextMenuCoordinator(mActivity, mMultiInstanceManager);
        when(mRectProvider.getRect())
                .thenReturn(new Rect(10, 10, mActivity.getWindow().getDecorView().getWidth(), 50));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ROBUST_WINDOW_MANAGEMENT)
    public void showMenu_verifyMenuState() {
        // Arrange.
        MultiWindowUtils.setMultiInstanceApi31EnabledForTesting(true);

        // Act.
        mCoordinator.showMenu(mRectProvider, false, mActivity);

        // Verify.
        verifyMenuState(/* expectedNumItems= */ 1);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ROBUST_WINDOW_MANAGEMENT)
    public void showMenu_verifyMenuState_noMultiInstanceSupport() {
        // Arrange.
        MultiWindowUtils.setMultiInstanceApi31EnabledForTesting(false);

        // Act.
        mCoordinator.showMenu(mRectProvider, false, mActivity);

        // Verify.
        verifyMenuState(/* expectedNumItems= */ 0);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ROBUST_WINDOW_MANAGEMENT)
    public void showMenu_verifyNameWindowOption() {
        // Arrange.
        MultiWindowUtils.setMultiInstanceApi31EnabledForTesting(true);
        mCoordinator.showMenu(mRectProvider, false, mActivity);
        verifyMenuState(/* expectedNumItems= */ 1);
        assertEquals(
                R.string.menu_name_window,
                getItemModelAtPosition(0).get(ListMenuItemProperties.TITLE_ID));

        // Act: Select "name window" option.
        mCoordinator
                .getListMenuDelegate(mContentView)
                .onItemSelected(getItemModelAtPosition(0), mListView);

        // Verify.
        verify(mMultiInstanceManager).showNameWindowDialog(NameWindowDialogSource.TAB_STRIP);
        assertFalse(mMenuWindow.isShowing());
    }

    private void verifyMenuState(int expectedNumItems) {
        mMenuWindow = mCoordinator.getPopupWindow();
        assertNotNull(mMenuWindow);
        if (expectedNumItems > 0) {
            assertTrue(mMenuWindow.isShowing());
            mContentView = mMenuWindow.getContentView();
            mListView = mContentView.findViewById(R.id.tab_group_action_menu_list);
            var adapter = (ModelListAdapter) mListView.getAdapter();
            assertEquals(expectedNumItems, adapter.getCount());
        } else {
            assertFalse(mMenuWindow.isShowing());
        }
    }

    private PropertyModel getItemModelAtPosition(int position) {
        var adapter = (ModelListAdapter) mListView.getAdapter();
        return ((ListItem) adapter.getItem(position)).model;
    }
}
