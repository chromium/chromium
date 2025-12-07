// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.magic_stack;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.reset;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.content.Context;
import android.graphics.Rect;
import android.view.ContextThemeWrapper;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.TextView;

import androidx.test.core.app.ApplicationProvider;
import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.chrome.browser.magic_stack.HomeModulesContextMenuManager.ContextMenuItemId;
import org.chromium.chrome.browser.magic_stack.ModuleDelegate.ModuleType;
import org.chromium.ui.listmenu.BasicListMenu;
import org.chromium.ui.widget.AnchoredPopupWindow;
import org.chromium.ui.widget.AnchoredPopupWindow.VerticalOrientation;

/** Unit tests for {@link HomeModulesContextMenuManager}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class HomeModulesContextMenuManagerUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    private static final String EXPECTED_OPTION_1 = "Hide Chrome tips card";
    private static final String EXPECTED_OPTION_2 = "More settings";
    private static final int MODULE_TYPE = ModuleType.PRICE_CHANGE;
    @Mock private ModuleProvider mModuleProvider;
    @Mock private View mView;
    @Mock private ModuleDelegate mModuleDelegate;
    @Mock private AnchoredPopupWindow mPopupWindow;
    @Mock private Runnable mDismissPopupWindowRunnable;
    @Captor private ArgumentCaptor<AnchoredPopupWindow.LayoutObserver> mLayoutObserverCaptor;
    private Context mContext;
    private BasicListMenu mMenu;
    private HomeModulesContextMenuManager mManager;

    @Before
    public void setUp() {
        mContext =
                new ContextThemeWrapper(
                        ApplicationProvider.getApplicationContext(),
                        R.style.Theme_BrowserUI_DayNight);
        mManager = new HomeModulesContextMenuManager(mModuleDelegate);

        when(mModuleProvider.getModuleContextMenuHideText(any(Context.class)))
                .thenReturn(EXPECTED_OPTION_1);
        when(mView.getContext()).thenReturn(mContext);
        when(mModuleProvider.getModuleType()).thenReturn(MODULE_TYPE);

        mMenu =
                mManager.getListMenu(
                        mView, mModuleProvider, mModuleDelegate, mDismissPopupWindowRunnable);

        mManager.setPopupWindowForTesting(mPopupWindow);
    }

    @Test
    @SmallTest
    public void testDisplayMenu() {
        View view =
                LayoutInflater.from(mContext).inflate(R.layout.educational_tip_module_layout, null);

        // Verifies notifyContextMenuShown(moduleProvider) is called
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                "MagicStack.Clank.NewTabPage.ContextMenu.ShownV2", MODULE_TYPE)
                        .build();

        mManager.displayMenu(view, mModuleProvider);

        histogramWatcher.assertExpected();
        verify(mModuleProvider).onContextMenuCreated();
    }

    @Test
    @SmallTest
    public void testGetListMenuContent() {
        // Obtains the first and second list views created by mManager.getListMenu().
        View listItemView1 =
                mMenu.getContentAdapter().getView(0, null, mMenu.getListView());
        View listItemView2 =
                mMenu.getContentAdapter().getView(1, null, mMenu.getListView());
        String listItemText1 =
                ((TextView) listItemView1.findViewById(R.id.menu_item_text)).getText().toString();
        String listItemText2 =
                ((TextView) listItemView2.findViewById(R.id.menu_item_text)).getText().toString();

        // Verifies if the texts inside views are expected.
        assertEquals(EXPECTED_OPTION_1, listItemText1);
        assertEquals(EXPECTED_OPTION_2, listItemText2);
    }

    @Test
    @SmallTest
    public void testGetListMenuDelegation() {
        // Verifies the magic stack module is removed, the action is recorded in the histogram and
        // the popup window is dismissed at the end of the clicking the first option.
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                "MagicStack.Clank.NewTabPage.ContextMenu.RemoveModuleV2",
                                MODULE_TYPE)
                        .build();

        mMenu.clickItemForTesting(0);

        histogramWatcher.assertExpected();
        verify(mModuleDelegate).removeModuleAndDisable(eq(MODULE_TYPE));
        verify(mDismissPopupWindowRunnable).run();
        reset(mDismissPopupWindowRunnable);

        // Verifies the customize setting page is opened, this action is recorded and the pop window
        // is dismissed at the end of the clicking the second option.
        histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                "MagicStack.Clank.NewTabPage.ContextMenu.OpenCustomizeSettings",
                                MODULE_TYPE)
                        .build();

        mMenu.clickItemForTesting(1);

        histogramWatcher.assertExpected();
        verify(mModuleDelegate).customizeSettings();
        verify(mDismissPopupWindowRunnable).run();
    }

    @Test
    @SmallTest
    public void testDismissPopupWindow() {
        mManager.dismissPopupWindow();
        verify(mPopupWindow).dismiss();
    }

    @Test
    @SmallTest
    public void testShowContextMenu() {
        mManager.showContextMenu(mMenu, mView);

        // Verifies that all necessary attributes of the popWindow are set.
        verify(mPopupWindow).setLayoutObserver(mLayoutObserverCaptor.capture());
        mLayoutObserverCaptor.getValue().onPreLayoutChange(true, 1, 1, 1, 1, new Rect(1, 2, 3, 4));
        verify(mPopupWindow).setAnimationStyle(R.style.StartIconMenuAnim);
        verify(mPopupWindow).setVerticalOverlapAnchor(eq(true));
        verify(mPopupWindow).setFocusable(eq(true));
        verify(mPopupWindow).setOutsideTouchable(eq(true));
        verify(mPopupWindow)
                .setPreferredHorizontalOrientation(
                        eq(AnchoredPopupWindow.HorizontalOrientation.CENTER));
        verify(mPopupWindow).setPreferredVerticalOrientation(eq(VerticalOrientation.BELOW));
        verify(mPopupWindow).show();
    }

    @Test
    @SmallTest
    public void testShowContextMenuWithShortMenuItem() {
        int leftPadding = 10;
        int rightPadding = 8;
        BasicListMenu menu = setMenuData(leftPadding, rightPadding, 5, 100);

        mManager.showContextMenu(menu, mView);

        // Verifies that the width of the popup window is the width of the longest item on the menu
        // plus paddings
        verify(mPopupWindow)
                .setDesiredContentWidth(eq(menu.getMaxItemWidth() + leftPadding + rightPadding));
    }

    @Test
    @SmallTest
    public void testShowContextMenuWithLongMenuItem() {
        int viewWidth = 100;
        BasicListMenu menu = setMenuData(10, 8, 100, viewWidth);
        mManager.showContextMenu(menu, mView);

        // Verifies that the width of the popup window is the width of the magic stack module
        // clicked
        verify(mPopupWindow).setDesiredContentWidth(eq(viewWidth));
    }

    @Test
    @SmallTest
    public void testShouldShowItem() {
        // Verifies that the "more settings" and "hide" menu items are default shown for all
        // modules.
        assertTrue(
                mManager.shouldShowItem(
                        ContextMenuItemId.SHOW_CUSTOMIZE_SETTINGS, mModuleProvider));
        assertTrue(mManager.shouldShowItem(ContextMenuItemId.HIDE_MODULE, mModuleProvider));

        // Cases for a customized menu item.
        when(mModuleProvider.isContextMenuItemSupported(2)).thenReturn(false);
        assertFalse(mManager.shouldShowItem(2, mModuleProvider));

        when(mModuleProvider.isContextMenuItemSupported(2)).thenReturn(true);
        assertTrue(mManager.shouldShowItem(2, mModuleProvider));
    }

    @Test
    @SmallTest
    public void testGetAnchorRectangle() {
        int width = 440;
        int height = 897;
        int locationHorizontal = 1808;
        int locationVertical = 32423;

        doAnswer(
                        invocation -> {
                            int[] list = invocation.getArgument(0);
                            list[0] = locationHorizontal;
                            list[1] = locationVertical;
                            return null;
                        })
                .when(mView)
                .getLocationOnScreen(any(int[].class));
        when(mView.getWidth()).thenReturn(width);
        when(mView.getHeight()).thenReturn(height);

        Rect anchorRect = mManager.getAnchorRectangle(mView);

        assertEquals(anchorRect.left, locationHorizontal);
        assertEquals(anchorRect.right, locationHorizontal + width);
        assertEquals(anchorRect.top, locationVertical + height / 4);
        assertEquals(anchorRect.bottom, locationVertical + height / 4 + height);
    }

    private BasicListMenu setMenuData(
            int leftPadding, int rightPadding, int maxWidth, int viewWidth) {
        View contentView = mock(View.class);
        BasicListMenu menu = mock(BasicListMenu.class);

        when(menu.getContentView()).thenReturn(contentView);
        when(contentView.getPaddingLeft()).thenReturn(leftPadding);
        when(contentView.getPaddingRight()).thenReturn(rightPadding);
        when(menu.getMaxItemWidth()).thenReturn(maxWidth);
        when(mView.getWidth()).thenReturn(viewWidth);

        return menu;
    }
}
