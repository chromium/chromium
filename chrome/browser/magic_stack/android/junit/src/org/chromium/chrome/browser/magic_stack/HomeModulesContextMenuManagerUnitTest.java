// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.magic_stack;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.never;
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
    @Mock private HomeModulesConfigManager mHomeModulesConfigManager;

    private @ModuleType int mModuleType;
    private Point mPoint = new Point(0, 0);
    private HomeModulesContextMenuManager mManager;

    @Before
    public void setUp() {
        mModuleType = ModuleType.SINGLE_TAB;
        doReturn(mContext).when(mView).getContext();
        doReturn(mModuleType).when(mModuleProvider).getModuleType();
        mManager =
                new HomeModulesContextMenuManager(
                        mModuleDelegate, mPoint, mHomeModulesConfigManager);
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
        // Verifies that the "customize" menu item is default shown for all modules.
        when(mHomeModulesConfigManager.hasModuleShownInSettings()).thenReturn(true);
        assertTrue(
                mManager.shouldShowItem(
                        ContextMenuItemId.SHOW_CUSTOMIZE_SETTINGS, mModuleProvider));

        // Verifies that the "customize" menu item is removed when there isn't any module to
        // customize.
        when(mHomeModulesConfigManager.hasModuleShownInSettings()).thenReturn(false);
        mManager.resetHasModuleToCustomizeForTesting();
        assertFalse(
                mManager.shouldShowItem(
                        ContextMenuItemId.SHOW_CUSTOMIZE_SETTINGS, mModuleProvider));

        // Verifies that the "hide module" menu item is shown for all modules except the single tab
        // module.
        assertEquals(ModuleType.SINGLE_TAB, mModuleProvider.getModuleType());
        assertFalse(mManager.shouldShowItem(ContextMenuItemId.HIDE_MODULE, mModuleProvider));
        when(mModuleProvider.getModuleType()).thenReturn(ModuleType.PRICE_CHANGE);
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

        when(mHomeModulesConfigManager.hasModuleShownInSettings()).thenReturn(false);
        mManager.createContextMenu(mContextMenu, mView, mModuleProvider);
        verify(menuItem1, never()).setOnMenuItemClickListener(any());
        verify(mModuleProvider, never()).onContextMenuCreated();

        when(mHomeModulesConfigManager.hasModuleShownInSettings()).thenReturn(true);
        mManager.resetHasModuleToCustomizeForTesting();
        mManager.createContextMenu(mContextMenu, mView, mModuleProvider);
        verify(menuItem1).setOnMenuItemClickListener(any());
        verify(mModuleProvider).onContextMenuCreated();
    }
}
