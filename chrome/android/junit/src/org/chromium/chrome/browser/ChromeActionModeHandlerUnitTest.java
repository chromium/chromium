// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import android.app.Activity;
import android.content.Intent;
import android.content.pm.ActivityInfo;
import android.content.pm.ResolveInfo;
import android.view.ActionMode;
import android.view.Menu;
import android.view.MenuItem;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.Shadows;
import org.robolectric.annotation.Config;
import org.robolectric.fakes.RoboMenu;
import org.robolectric.shadows.ShadowPackageManager;

import org.chromium.base.Callback;
import org.chromium.base.PackageManagerUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.firstrun.FirstRunStatus;
import org.chromium.chrome.browser.locale.LocaleManager;
import org.chromium.chrome.browser.locale.LocaleManagerDelegate;
import org.chromium.chrome.browser.readaloud.ReadAloudController;
import org.chromium.chrome.browser.share.ShareDelegate;
import org.chromium.chrome.browser.share.ShareDelegate.ShareOrigin;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.content.R;
import org.chromium.content_public.browser.ActionModeCallbackHelper;
import org.chromium.content_public.browser.WebContents;

import java.util.ArrayList;
import java.util.Collections;
import java.util.LinkedList;
import java.util.List;
import java.util.Random;

/** Unit tests for the {@link ChromeActionModeHandler}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class ChromeActionModeHandlerUnitTest {
    @Mock private Tab mTab;
    @Mock private ActionModeCallbackHelper mActionModeCallbackHelper;
    @Mock private ActionMode mActionMode;
    @Mock private Menu mMenu;
    @Mock private ShareDelegate mShareDelegate;
    @Mock private ReadAloudController mReadAloudController;

    private class TestChromeActionModeCallback
            extends ChromeActionModeHandler.ChromeActionModeCallback {
        TestChromeActionModeCallback(Tab tab, ActionModeCallbackHelper helper) {
            super(
                    tab,
                    null,
                    urlParams -> {},
                    true,
                    () -> mShareDelegate,
                    () -> mReadAloudController);
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

        mActionModeCallback =
                Mockito.spy(new TestChromeActionModeCallback(mTab, mActionModeCallbackHelper));
    }

    @After
    public void tearDown() {
        FirstRunStatus.setFirstRunFlowComplete(false);
    }

    @Test
    public void testOptionsBeforeFre() {
        FirstRunStatus.setFirstRunFlowComplete(false);

        mActionModeCallback.onCreateActionMode(mActionMode, mMenu);

        Mockito.verify(mActionModeCallbackHelper)
                .setAllowedMenuItems(
                        ActionModeCallbackHelper.MENU_ITEM_PROCESS_TEXT
                                | ActionModeCallbackHelper.MENU_ITEM_SHARE);
    }

    @Test
    public void testOptionsAfterFre() {
        FirstRunStatus.setFirstRunFlowComplete(true);

        mActionModeCallback.onCreateActionMode(mActionMode, mMenu);

        Mockito.verify(mActionModeCallbackHelper)
                .setAllowedMenuItems(
                        ActionModeCallbackHelper.MENU_ITEM_PROCESS_TEXT
                                | ActionModeCallbackHelper.MENU_ITEM_SHARE
                                | ActionModeCallbackHelper.MENU_ITEM_WEB_SEARCH);
    }

    @Test
    public void testShareTriggersSearchPromo() {
        FirstRunStatus.setFirstRunFlowComplete(true);

        Mockito.when(mActionModeCallbackHelper.isActionModeValid()).thenReturn(true);
        Mockito.when(mActionModeCallbackHelper.getSelectedText()).thenReturn("OhHai");

        LocaleManagerDelegate delegate =
                Mockito.spy(
                        new LocaleManagerDelegate() {
                            @Override
                            public void showSearchEnginePromoIfNeeded(
                                    Activity activity, Callback<Boolean> onSearchEngineFinalized) {
                                onSearchEngineFinalized.onResult(true);
                            }
                        });

        LocaleManager.getInstance().setDelegateForTest(delegate);

        MenuItem shareItem = Mockito.mock(MenuItem.class);
        Mockito.when(shareItem.getItemId()).thenReturn(R.id.select_action_menu_web_search);
        mActionModeCallback.onActionItemClicked(mActionMode, shareItem);

        Mockito.verify(delegate).showSearchEnginePromoIfNeeded(Mockito.any(), Mockito.any());
    }

    @Test
    public void testSelectActionMenuTextProcessingMenus() {
        ShadowPackageManager packageManager =
                Shadows.shadowOf(RuntimeEnvironment.application.getPackageManager());
        List<String> browserPackageNames = new ArrayList<>();
        List<String> launcherPackageNames = new ArrayList<>();
        List<String> otherPackageNames = new ArrayList<>();
        List<ResolveInfo> browsersList = new LinkedList<>();
        List<ResolveInfo> launchersList = new LinkedList<>();
        for (int i = 0; i < 5; i++) {
            browserPackageNames.add("foo " + i);
            browsersList.add(createResolveInfo(browserPackageNames.get(i)));
            launcherPackageNames.add("bar " + i);
            launchersList.add(createResolveInfo(launcherPackageNames.get(i)));
            otherPackageNames.add("baz " + i);
        }

        // Mock intent for querying web browsers.
        packageManager.addResolveInfoForIntent(PackageManagerUtils.BROWSER_INTENT, browsersList);

        // Mock intent for querying home launchers.
        packageManager.addResolveInfoForIntent(
                PackageManagerUtils.getQueryInstalledHomeLaunchersIntent(), launchersList);

        RoboMenu menu = new RoboMenu(RuntimeEnvironment.application);

        List<String> allNames = new LinkedList<>();
        allNames.addAll(browserPackageNames);
        allNames.addAll(launcherPackageNames);
        allNames.addAll(otherPackageNames);
        // Shuffle the list to get it closer to the reality.
        Collections.shuffle(allNames, new Random(42));
        for (int i = 0; i < allNames.size(); i++) {
            addMenuItem(menu, i, allNames.get(i));
        }

        mActionModeCallback.onPrepareActionMode(mActionMode, menu);

        // Verify that some menu items have been made invisible.
        for (int i = 0; i < menu.size(); i++) {
            MenuItem item = menu.getItem(i);
            if (item.getIntent() == null || item.getIntent().getComponent() == null) continue;
            String packageName = item.getIntent().getComponent().getPackageName();
            if (browserPackageNames.contains(packageName)
                    || launcherPackageNames.contains(packageName)) {
                Assert.assertFalse(
                        "Browser or home launcher application should be filtered out",
                        item.isVisible());
            } else {
                Assert.assertTrue(
                        "Actions other than browsers or home launchers should not be filtered out",
                        item.isVisible());
            }
        }
    }

    @Test
    public void testShare() {
        Mockito.when(mActionModeCallbackHelper.isActionModeValid()).thenReturn(true);
        MenuItem shareItem = Mockito.mock(MenuItem.class);
        Mockito.when(shareItem.getItemId()).thenReturn(R.id.select_action_menu_share);
        mActionModeCallback.onActionItemClicked(mActionMode, shareItem);

        Mockito.verify(mShareDelegate).share(any(), any(), eq(ShareOrigin.MOBILE_ACTION_MODE));
        Mockito.verify(mActionModeCallbackHelper, times(0)).onActionItemClicked(any(), any());
    }

    @Test
    public void testShareWithoutShareDelegate() {
        mShareDelegate = null;

        Mockito.when(mActionModeCallbackHelper.isActionModeValid()).thenReturn(true);
        MenuItem shareItem = Mockito.mock(MenuItem.class);
        Mockito.when(shareItem.getItemId()).thenReturn(R.id.select_action_menu_share);
        mActionModeCallback.onActionItemClicked(mActionMode, shareItem);

        Mockito.verify(mActionModeCallbackHelper).onActionItemClicked(any(), eq(shareItem));
    }

    @Test
    public void testMaybePauseReadAloudOnActionItemClicked() {
        Mockito.when(mActionModeCallbackHelper.isActionModeValid()).thenReturn(true);
        MenuItem item = Mockito.mock(MenuItem.class);
        Intent intent = new Intent();
        doReturn(intent).when(item).getIntent();

        mActionModeCallback.onActionItemClicked(mActionMode, item);
        verify(mReadAloudController).maybePauseForOutgoingIntent(eq(intent));
    }

    private ResolveInfo createResolveInfo(String packageName) {
        ResolveInfo resolveInfo = new ResolveInfo();
        ActivityInfo activityInfo = new ActivityInfo();
        activityInfo.packageName = packageName;
        resolveInfo.activityInfo = activityInfo;
        return resolveInfo;
    }

    private void addMenuItem(Menu menu, int order, String packageName) {
        menu.add(R.id.select_action_menu_text_processing_items, Menu.NONE, order, "title")
                .setIntent(
                        new Intent()
                                .setAction(Intent.ACTION_PROCESS_TEXT)
                                .setType("text/plain")
                                .putExtra(Intent.EXTRA_PROCESS_TEXT_READONLY, true)
                                .setClassName(packageName, "foo"));
    }
}
