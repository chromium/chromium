// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.mockito.Mockito.eq;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.content.Context;
import android.graphics.drawable.Drawable;

import androidx.appcompat.content.res.AppCompatResources;
import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.annotation.Config;

import org.chromium.base.supplier.Supplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.chrome.browser.share.ChromeShareExtras;
import org.chromium.chrome.browser.share.ShareDelegate;
import org.chromium.chrome.browser.share.ShareDelegate.ShareOrigin;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tasks.tab_management.TabSelectionEditorAction.ActionDelegate;
import org.chromium.chrome.browser.tasks.tab_management.TabSelectionEditorAction.ActionObserver;
import org.chromium.chrome.browser.tasks.tab_management.TabSelectionEditorAction.ButtonType;
import org.chromium.chrome.browser.tasks.tab_management.TabSelectionEditorAction.IconPosition;
import org.chromium.chrome.browser.tasks.tab_management.TabSelectionEditorAction.ShowMode;
import org.chromium.chrome.tab_ui.R;
import org.chromium.chrome.test.util.browser.tabmodel.MockTabModel;
import org.chromium.components.browser_ui.share.ShareParams;
import org.chromium.components.browser_ui.widget.selectable_list.SelectionDelegate;

import java.util.ArrayList;
import java.util.LinkedHashSet;
import java.util.List;
import java.util.Set;

/**
 * Unit tests for {@link TabSelectionEditorShareAction}.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class TabSelectionEditorShareActionUnitTest {
    @Mock
    private TabModelSelector mTabModelSelector;
    @Mock
    private SelectionDelegate<Integer> mSelectionDelegate;
    @Mock
    private ActionDelegate mDelegate;
    @Mock
    private Supplier<ShareDelegate> mShareDelegateSupplier;
    @Mock
    private ShareDelegate mShareDelegate;
    private Context mContext;
    private MockTabModel mTabModel;
    private TabSelectionEditorAction mAction;

    @Captor
    ArgumentCaptor<ShareParams> mShareParamsCaptor;
    @Captor
    ArgumentCaptor<ChromeShareExtras> mChromeShareExtrasCaptor;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mContext = RuntimeEnvironment.application;
        mAction = TabSelectionEditorShareAction.createAction(mContext, ShowMode.MENU_ONLY,
                ButtonType.TEXT, IconPosition.START, mShareDelegateSupplier);
        mTabModel = spy(new MockTabModel(false, null));
        when(mTabModelSelector.getCurrentModel()).thenReturn(mTabModel);
        mAction.configure(mTabModelSelector, mSelectionDelegate, mDelegate, false);
    }

    @Test
    @SmallTest
    public void testInherentActionProperties() {
        Drawable drawable = AppCompatResources.getDrawable(
                mContext, R.drawable.tab_selection_editor_share_icon);
        drawable.setBounds(0, 0, drawable.getIntrinsicWidth(), drawable.getIntrinsicHeight());

        Assert.assertEquals(R.id.tab_selection_editor_share_menu_item,
                mAction.getPropertyModel().get(TabSelectionEditorActionProperties.MENU_ITEM_ID));
        Assert.assertEquals(R.plurals.tab_selection_editor_share_tabs_action_button,
                mAction.getPropertyModel().get(
                        TabSelectionEditorActionProperties.TITLE_RESOURCE_ID));
        Assert.assertEquals(true,
                mAction.getPropertyModel().get(TabSelectionEditorActionProperties.TITLE_IS_PLURAL));
        Assert.assertEquals(R.plurals.accessibility_tab_selection_editor_share_tabs_action_button,
                mAction.getPropertyModel()
                        .get(TabSelectionEditorActionProperties.CONTENT_DESCRIPTION_RESOURCE_ID)
                        .intValue());
        Assert.assertNotNull(
                mAction.getPropertyModel().get(TabSelectionEditorActionProperties.ICON));
    }

    @Test
    @SmallTest
    public void testShareActionNoTabs() {
        mAction.onSelectionStateChange(new ArrayList<Integer>());
        Assert.assertEquals(
                false, mAction.getPropertyModel().get(TabSelectionEditorActionProperties.ENABLED));
        Assert.assertEquals(
                0, mAction.getPropertyModel().get(TabSelectionEditorActionProperties.ITEM_COUNT));
    }

    @Test
    @SmallTest
    public void testShareActionWithOneTab() throws Exception {
        List<Integer> tabIds = new ArrayList<>();
        tabIds.add(5);

        List<Tab> tabs = new ArrayList<>();
        for (int id : tabIds) {
            tabs.add(mTabModel.addTab(id));
        }
        Set<Integer> tabIdsSet = new LinkedHashSet<>(tabIds);
        when(mSelectionDelegate.getSelectedItems()).thenReturn(tabIdsSet);

        mAction.onSelectionStateChange(tabIds);
        Assert.assertEquals(
                true, mAction.getPropertyModel().get(TabSelectionEditorActionProperties.ENABLED));
        Assert.assertEquals(
                1, mAction.getPropertyModel().get(TabSelectionEditorActionProperties.ITEM_COUNT));

        ShareParams shareParams =
                new ShareParams
                        .Builder(tabs.get(0).getWindowAndroid(), tabs.get(0).getTitle(),
                                tabs.get(0).getUrl().getSpec())
                        .setText("")
                        .build();
        ChromeShareExtras chromeShareExtras = new ChromeShareExtras.Builder()
                                                      .setSharingTabGroup(true)
                                                      .setSaveLastUsed(true)
                                                      .build();

        final CallbackHelper helper = new CallbackHelper();
        ActionObserver observer = new ActionObserver() {
            @Override
            public void preProcessSelectedTabs(List<Tab> tabs) {
                helper.notifyCalled();
            }
        };
        mAction.addActionObserver(observer);

        when(mShareDelegateSupplier.get()).thenReturn(mShareDelegate);
        Assert.assertTrue(mAction.perform());

        verify(mShareDelegate)
                .share(mShareParamsCaptor.capture(), mChromeShareExtrasCaptor.capture(),
                        eq(ShareOrigin.TAB_GROUP));
        ShareParams shareParamsCaptorValue = mShareParamsCaptor.getValue();
        ChromeShareExtras chromeShareExtrasCaptorValue = mChromeShareExtrasCaptor.getValue();

        Assert.assertEquals(shareParams.getWindow(), shareParamsCaptorValue.getWindow());
        Assert.assertEquals(shareParams.getTitle(), shareParamsCaptorValue.getTitle());
        Assert.assertEquals(shareParams.getUrl(), shareParamsCaptorValue.getUrl());
        Assert.assertEquals(shareParams.getText(), shareParamsCaptorValue.getText());
        Assert.assertEquals(chromeShareExtras.sharingTabGroup(),
                chromeShareExtrasCaptorValue.sharingTabGroup());
        Assert.assertEquals(
                chromeShareExtras.saveLastUsed(), chromeShareExtrasCaptorValue.saveLastUsed());

        helper.waitForFirst();
        mAction.removeActionObserver(observer);

        Assert.assertTrue(mAction.perform());
        verify(mShareDelegate, times(2))
                .share(mShareParamsCaptor.capture(), mChromeShareExtrasCaptor.capture(),
                        eq(ShareOrigin.TAB_GROUP));
        shareParamsCaptorValue = mShareParamsCaptor.getValue();
        chromeShareExtrasCaptorValue = mChromeShareExtrasCaptor.getValue();
        Assert.assertEquals(shareParams.getWindow(), shareParamsCaptorValue.getWindow());
        Assert.assertEquals(shareParams.getTitle(), shareParamsCaptorValue.getTitle());
        Assert.assertEquals(shareParams.getUrl(), shareParamsCaptorValue.getUrl());
        Assert.assertEquals(shareParams.getText(), shareParamsCaptorValue.getText());
        Assert.assertEquals(chromeShareExtras.sharingTabGroup(),
                chromeShareExtrasCaptorValue.sharingTabGroup());
        Assert.assertEquals(
                chromeShareExtras.saveLastUsed(), chromeShareExtrasCaptorValue.saveLastUsed());
        Assert.assertEquals(1, helper.getCallCount());
    }

    @Test
    @SmallTest
    public void testShareActionWithMultipleTabs() throws Exception {
        List<Integer> tabIds = new ArrayList<>();
        tabIds.add(5);
        tabIds.add(3);
        tabIds.add(7);
        List<Tab> tabs = new ArrayList<>();
        for (int id : tabIds) {
            tabs.add(mTabModel.addTab(id));
        }
        Set<Integer> tabIdsSet = new LinkedHashSet<>(tabIds);
        when(mSelectionDelegate.getSelectedItems()).thenReturn(tabIdsSet);

        mAction.onSelectionStateChange(tabIds);
        Assert.assertEquals(
                true, mAction.getPropertyModel().get(TabSelectionEditorActionProperties.ENABLED));
        Assert.assertEquals(
                3, mAction.getPropertyModel().get(TabSelectionEditorActionProperties.ITEM_COUNT));

        ShareParams shareParams = new ShareParams.Builder(tabs.get(0).getWindowAndroid(), "", "")
                                          .setText("1. \n2. \n3. \n")
                                          .build();
        ChromeShareExtras chromeShareExtras = new ChromeShareExtras.Builder()
                                                      .setSharingTabGroup(true)
                                                      .setSaveLastUsed(true)
                                                      .build();

        final CallbackHelper helper = new CallbackHelper();
        ActionObserver observer = new ActionObserver() {
            @Override
            public void preProcessSelectedTabs(List<Tab> tabs) {
                helper.notifyCalled();
            }
        };
        mAction.addActionObserver(observer);

        when(mShareDelegateSupplier.get()).thenReturn(mShareDelegate);
        Assert.assertTrue(mAction.perform());

        verify(mShareDelegate)
                .share(mShareParamsCaptor.capture(), mChromeShareExtrasCaptor.capture(),
                        eq(ShareOrigin.TAB_GROUP));
        ShareParams shareParamsCaptorValue = mShareParamsCaptor.getValue();
        ChromeShareExtras chromeShareExtrasCaptorValue = mChromeShareExtrasCaptor.getValue();
        Assert.assertEquals(shareParams.getWindow(), shareParamsCaptorValue.getWindow());
        Assert.assertEquals(shareParams.getTitle(), shareParamsCaptorValue.getTitle());
        Assert.assertEquals(shareParams.getUrl(), shareParamsCaptorValue.getUrl());
        Assert.assertEquals(shareParams.getText(), shareParamsCaptorValue.getText());
        Assert.assertEquals(chromeShareExtras.sharingTabGroup(),
                chromeShareExtrasCaptorValue.sharingTabGroup());
        Assert.assertEquals(
                chromeShareExtras.saveLastUsed(), chromeShareExtrasCaptorValue.saveLastUsed());

        helper.waitForFirst();
        mAction.removeActionObserver(observer);

        Assert.assertTrue(mAction.perform());
        verify(mShareDelegate, times(2))
                .share(mShareParamsCaptor.capture(), mChromeShareExtrasCaptor.capture(),
                        eq(ShareOrigin.TAB_GROUP));
        shareParamsCaptorValue = mShareParamsCaptor.getValue();
        chromeShareExtrasCaptorValue = mChromeShareExtrasCaptor.getValue();
        Assert.assertEquals(shareParams.getWindow(), shareParamsCaptorValue.getWindow());
        Assert.assertEquals(shareParams.getTitle(), shareParamsCaptorValue.getTitle());
        Assert.assertEquals(shareParams.getUrl(), shareParamsCaptorValue.getUrl());
        Assert.assertEquals(shareParams.getText(), shareParamsCaptorValue.getText());
        Assert.assertEquals(chromeShareExtras.sharingTabGroup(),
                chromeShareExtrasCaptorValue.sharingTabGroup());
        Assert.assertEquals(
                chromeShareExtras.saveLastUsed(), chromeShareExtrasCaptorValue.saveLastUsed());
        Assert.assertEquals(1, helper.getCallCount());
    }
}
