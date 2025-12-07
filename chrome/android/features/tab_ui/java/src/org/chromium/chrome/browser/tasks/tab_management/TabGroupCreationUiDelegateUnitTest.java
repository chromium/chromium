// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;

import androidx.test.ext.junit.rules.ActivityScenarioRule;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.Token;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.hub.PaneId;
import org.chromium.chrome.browser.hub.PaneManager;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabCreator;
import org.chromium.chrome.browser.tabmodel.TabGroupModelFilter;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.modaldialog.ModalDialogManager;

import java.util.concurrent.atomic.AtomicReference;
import java.util.function.Supplier;

/** Unit tests for {@link TabGroupCreationUiDelegate}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class TabGroupCreationUiDelegateUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule
    public ActivityScenarioRule<TestActivity> mActivityScenarioRule =
            new ActivityScenarioRule<>(TestActivity.class);

    @Mock private ModalDialogManager mModalDialogManager;
    @Mock private TabGroupCreationDialogManager mTabGroupCreationDialogManager;
    @Mock private PaneManager mPaneManager;
    @Mock private TabGroupModelFilter mFilter;
    @Mock private TabModel mTabModel;
    @Mock private TabCreator mTabCreator;
    @Mock private Tab mTab;
    @Mock private TabSwitcherPaneBase mTabSwitcherPane;

    private Supplier<ModalDialogManager> mModalDialogManagerSupplier;
    private Supplier<PaneManager> mPaneManagerSupplier;
    private Supplier<TabGroupModelFilter> mFilterSupplier;
    private TabGroupCreationUiDelegate mTabGroupCreationUiDelegate;
    private Token mToken;
    private Activity mActivity;

    @Before
    public void setUp() {
        mActivityScenarioRule.getScenario().onActivity(activity -> mActivity = activity);
        mModalDialogManagerSupplier = () -> mModalDialogManager;
        mPaneManagerSupplier = () -> mPaneManager;
        mFilterSupplier = () -> mFilter;

        when(mFilter.getTabModel()).thenReturn(mTabModel);
        when(mTabModel.getTabCreator()).thenReturn(mTabCreator);
        when(mTabModel.isIncognitoBranded()).thenReturn(false);
        mToken = Token.createRandom();
        when(mTab.getTabGroupId()).thenReturn(mToken);
        when(mTab.getId()).thenReturn(1);

        mTabGroupCreationUiDelegate =
                new TabGroupCreationUiDelegate(
                        mActivity,
                        mModalDialogManagerSupplier,
                        mPaneManagerSupplier,
                        mFilterSupplier,
                        (a, b, c) -> mTabGroupCreationDialogManager);

        when(mTabCreator.createNewTab(any(), anyInt(), any())).thenReturn(mTab);
    }

    @Test
    public void testNewTabGroupFlow() {
        mTabGroupCreationUiDelegate.newTabGroupFlow();
        verify(mTabGroupCreationDialogManager).showDialog(mToken, mFilter);
    }

    @Test
    public void testNewTabGroupFlow_tabCreationFails() {
        when(mTabCreator.createNewTab(any(), anyInt(), any())).thenReturn(null);
        mTabGroupCreationUiDelegate.newTabGroupFlow();
        verify(mTabGroupCreationDialogManager, never()).showDialog(mToken, mFilter);
    }

    @Test
    public void testOpenTabGroupUi() {
        when(mPaneManager.focusPane(PaneId.TAB_SWITCHER)).thenReturn(true);
        when(mPaneManager.getPaneForId(PaneId.TAB_SWITCHER)).thenReturn(mTabSwitcherPane);

        AtomicReference<Runnable> openTabGroupUiContainer = new AtomicReference<>(() -> {});
        mTabGroupCreationUiDelegate =
                new TabGroupCreationUiDelegate(
                        mActivity,
                        mModalDialogManagerSupplier,
                        mPaneManagerSupplier,
                        mFilterSupplier,
                        (a, b, openTabGroupUi) -> {
                            openTabGroupUiContainer.set(openTabGroupUi);
                            return mTabGroupCreationDialogManager;
                        });
        mTabGroupCreationUiDelegate.newTabGroupFlow();
        openTabGroupUiContainer.get().run();
        verify(mTabSwitcherPane).requestOpenTabGroupDialog(1);
    }

    @Test
    public void testOpenTabGroupUi_Incognito() {
        when(mTabModel.isIncognitoBranded()).thenReturn(true);
        when(mPaneManager.focusPane(PaneId.INCOGNITO_TAB_SWITCHER)).thenReturn(true);
        when(mPaneManager.getPaneForId(PaneId.INCOGNITO_TAB_SWITCHER)).thenReturn(mTabSwitcherPane);

        AtomicReference<Runnable> openTabGroupUiContainer = new AtomicReference<>(() -> {});
        mTabGroupCreationUiDelegate =
                new TabGroupCreationUiDelegate(
                        mActivity,
                        mModalDialogManagerSupplier,
                        mPaneManagerSupplier,
                        mFilterSupplier,
                        (a, b, openTabGroupUi) -> {
                            openTabGroupUiContainer.set(openTabGroupUi);
                            return mTabGroupCreationDialogManager;
                        });
        mTabGroupCreationUiDelegate.newTabGroupFlow();
        openTabGroupUiContainer.get().run();
        verify(mTabSwitcherPane).requestOpenTabGroupDialog(1);
    }

    @Test
    public void testOpenTabGroupUi_noTabSwitcherPane() {
        when(mPaneManager.focusPane(PaneId.TAB_SWITCHER)).thenReturn(true);
        when(mPaneManager.getPaneForId(PaneId.TAB_SWITCHER)).thenReturn(null);

        AtomicReference<Runnable> openTabGroupUiContainer = new AtomicReference<>(() -> {});
        mTabGroupCreationUiDelegate =
                new TabGroupCreationUiDelegate(
                        mActivity,
                        mModalDialogManagerSupplier,
                        mPaneManagerSupplier,
                        mFilterSupplier,
                        (a, b, openTabGroupUi) -> {
                            openTabGroupUiContainer.set(openTabGroupUi);
                            return mTabGroupCreationDialogManager;
                        });
        mTabGroupCreationUiDelegate.newTabGroupFlow();
        openTabGroupUiContainer.get().run();

        verify(mPaneManager).focusPane(PaneId.TAB_SWITCHER);
        verify(mPaneManager).getPaneForId(PaneId.TAB_SWITCHER);
        verify(mTabSwitcherPane, never()).requestOpenTabGroupDialog(anyInt());
    }
}
