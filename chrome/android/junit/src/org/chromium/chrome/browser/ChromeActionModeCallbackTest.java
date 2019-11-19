// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import android.app.Activity;
import android.view.ActionMode;
import android.view.Menu;
import android.view.MenuItem;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.Callback;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.browser.firstrun.FirstRunStatus;
import org.chromium.chrome.browser.locale.LocaleManager;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.content.R;
import org.chromium.content_public.browser.ActionModeCallbackHelper;
import org.chromium.content_public.browser.WebContents;
import org.chromium.testing.local.LocalRobolectricTestRunner;

/**
 * Unit tests for the {@link ChromeActionModeCallback}.
 */
@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class ChromeActionModeCallbackTest {
    @Mock
    private Tab mTab;
    @Mock
    private ActionModeCallbackHelper mActionModeCallbackHelper;
    @Mock
    private ActionMode mActionMode;
    @Mock
    private Menu mMenu;

    private class TestChromeActionModeCallback extends ChromeActionModeCallback {
        public TestChromeActionModeCallback(Tab tab, ActionModeCallbackHelper helper) {
            super(tab, null);
        }

        @Override
        public ActionModeCallbackHelper getActionModeCallbackHelper(WebContents webContents) {
            return mActionModeCallbackHelper;
        }
    }

    private TestChromeActionModeCallback mActionModeCallback;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        RecordUserAction.setDisabledForTests(true);

        mActionModeCallback =
                Mockito.spy(new TestChromeActionModeCallback(mTab, mActionModeCallbackHelper));
    }

    @After
    public void tearDown() {
        FirstRunStatus.setFirstRunFlowComplete(false);
        RecordUserAction.setDisabledForTests(false);
    }

    @Test
    public void testOptionsBeforeFre() {
        FirstRunStatus.setFirstRunFlowComplete(false);

        mActionModeCallback.onCreateActionMode(mActionMode, mMenu);

        Mockito.verify(mActionModeCallbackHelper)
                .setAllowedMenuItems(ActionModeCallbackHelper.MENU_ITEM_PROCESS_TEXT
                        | ActionModeCallbackHelper.MENU_ITEM_SHARE);
    }

    @Test
    public void testOptionsAfterFre() {
        FirstRunStatus.setFirstRunFlowComplete(true);

        mActionModeCallback.onCreateActionMode(mActionMode, mMenu);

        Mockito.verify(mActionModeCallbackHelper)
                .setAllowedMenuItems(ActionModeCallbackHelper.MENU_ITEM_PROCESS_TEXT
                        | ActionModeCallbackHelper.MENU_ITEM_SHARE
                        | ActionModeCallbackHelper.MENU_ITEM_WEB_SEARCH);
    }

    @Test
    public void testShareTriggersSearchPromo() {
        FirstRunStatus.setFirstRunFlowComplete(true);

        Mockito.when(mActionModeCallbackHelper.isActionModeValid()).thenReturn(true);
        Mockito.when(mActionModeCallbackHelper.getSelectedText()).thenReturn("OhHai");

        LocaleManager localeManager = Mockito.spy(new LocaleManager() {
            @Override
            public void showSearchEnginePromoIfNeeded(
                    Activity activity, Callback<Boolean> onSearchEngineFinalized) {
                onSearchEngineFinalized.onResult(true);
            }
        });
        LocaleManager.setInstanceForTest(localeManager);

        MenuItem shareItem = Mockito.mock(MenuItem.class);
        Mockito.when(shareItem.getItemId()).thenReturn(R.id.select_action_menu_web_search);
        mActionModeCallback.onActionItemClicked(mActionMode, shareItem);

        Mockito.verify(localeManager).showSearchEnginePromoIfNeeded(Mockito.any(), Mockito.any());
    }
}
