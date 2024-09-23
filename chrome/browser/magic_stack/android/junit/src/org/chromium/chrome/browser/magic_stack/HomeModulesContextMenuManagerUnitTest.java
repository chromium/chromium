// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.magic_stack;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.anyString;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.content.Context;
import android.graphics.Point;
import android.view.ContextMenu;
import android.view.Menu;
import android.view.MenuItem;
import android.view.View;

import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.magic_stack.HomeModulesContextMenuManager.ContextMenuItemId;
import org.chromium.chrome.browser.magic_stack.ModuleDelegate.ModuleType;

/** Unit tests for {@link HomeModulesContextMenuManager}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class HomeModulesContextMenuManagerUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private ModuleDelegate mModuleDelegate;
    @Mock private ModuleProvider mModuleProvider;
    @Mock private MenuItem mMenuItem;
    @Mock private ContextMenu mContextMenu;
    @Mock private View mView;
    @Mock private Context mContext;

    private @ModuleType int mModuleType;
    private String mContextMenuHide = "Hide module";
    private Point mPoint = new Point(0, 0);
    private HomeModulesContextMenuManager mManager;

    @Before
    public void setUp() {
        mModuleType = ModuleType.SINGLE_TAB;
        doReturn(mContext).when(mView).getContext();
        doReturn(mModuleType).when(mModuleProvider).getModuleType();
        when(mModuleProvider.getModuleContextMenuHideText(any())).thenReturn(mContextMenuHide);
        mManager = new HomeModulesContextMenuManager(mModuleDelegate, mPoint);
    }

    @Test
    @SmallTest
    public void testOnMenuItemClick() {
        doReturn(ContextMenuItemId.HIDE_MODULE).when(mMenuItem).getItemId();
        mManager.onMenuItemClickImpl(mMenuItem, mModuleProvider);
        verify(mModuleDelegate).removeModuleAndDisable(eq(mModuleType));

        doReturn(ContextMenuItemId.SHOW_CUSTOMIZE_SETTINGS).when(mMenuItem).getItemId();
        mManager.onMenuItemClickImpl(mMenuItem, mModuleProvider);
        verify(mModuleDelegate).customizeSettings();
    }

    @Test
    @SmallTest
    public void testShouldShowItem() {
        // Verifies that the "customize" and "hide" menu items are default shown for all modules.
        assertTrue(
                mManager.shouldShowItem(
                        ContextMenuItemId.SHOW_CUSTOMIZE_SETTINGS, mModuleProvider));
        assertTrue(mManager.shouldShowItem(ContextMenuItemId.HIDE_MODULE, mModuleProvider));

        // Cases for a customized menu item.
        doReturn(false).when(mModuleProvider).isContextMenuItemSupported(2);
        assertFalse(mManager.shouldShowItem(2, mModuleProvider));

        doReturn(true).when(mModuleProvider).isContextMenuItemSupported(2);
        assertTrue(mManager.shouldShowItem(2, mModuleProvider));
    }

    @Test
    @SmallTest
    public void testCreateContextMenu() {
        MenuItem menuItem1 = Mockito.mock(MenuItem.class);
        doReturn(menuItem1)
                .when(mContextMenu)
                .add(
                        eq(Menu.NONE),
                        eq(ContextMenuItemId.SHOW_CUSTOMIZE_SETTINGS),
                        eq(Menu.NONE),
                        anyInt());
        MenuItem menuItem2 = Mockito.mock(MenuItem.class);
        when(mContextMenu.add(anyString())).thenReturn(menuItem2);

        mManager.createContextMenu(mContextMenu, mView, mModuleProvider);

        // Verifies context menu items SHOW_CUSTOMIZE_SETTINGS and HIDE_MODULE are shown.
        verify(menuItem1).setOnMenuItemClickListener(any());
        verify(menuItem2).setOnMenuItemClickListener(any());
        verify(mModuleProvider).onContextMenuCreated();
    }
}
