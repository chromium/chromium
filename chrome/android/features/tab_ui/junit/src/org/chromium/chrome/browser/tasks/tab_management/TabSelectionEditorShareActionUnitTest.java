// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.when;

import android.content.Context;
import android.content.Intent;
import android.graphics.drawable.Drawable;

import androidx.appcompat.content.res.AppCompatResources;
import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.tab.MockTab;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModelFilterProvider;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tasks.tab_management.TabSelectionEditorAction.ActionDelegate;
import org.chromium.chrome.browser.tasks.tab_management.TabSelectionEditorAction.ActionObserver;
import org.chromium.chrome.browser.tasks.tab_management.TabSelectionEditorAction.ButtonType;
import org.chromium.chrome.browser.tasks.tab_management.TabSelectionEditorAction.IconPosition;
import org.chromium.chrome.browser.tasks.tab_management.TabSelectionEditorAction.ShowMode;
import org.chromium.chrome.tab_ui.R;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.chrome.test.util.browser.tabmodel.MockTabModel;
import org.chromium.components.browser_ui.share.ShareParams;
import org.chromium.components.browser_ui.widget.selectable_list.SelectionDelegate;
import org.chromium.components.dom_distiller.core.DomDistillerUrlUtilsJni;
import org.chromium.url.GURL;
import org.chromium.url.JUnitTestGURLs;

import java.util.ArrayList;
import java.util.LinkedHashSet;
import java.util.List;
import java.util.Map;
import java.util.Set;

/**
 * Unit tests for {@link TabSelectionEditorShareAction}.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class TabSelectionEditorShareActionUnitTest {
    @Rule
    public JniMocker mJniMocker = new JniMocker();
    @Rule
    public TestRule mProcessor = new Features.JUnitProcessor();

    @Mock
    private TabModelSelector mTabModelSelector;
    @Mock
    private SelectionDelegate<Integer> mSelectionDelegate;
    @Mock
    private ActionDelegate mDelegate;
    @Mock
    private DomDistillerUrlUtilsJni mDomDistillerUrlUtilsJni;
    private Context mContext;
    private MockTabModel mTabModel;
    private TabSelectionEditorShareAction mAction;

    Map<Integer, GURL> mIdUrlMap = Map.of(1, JUnitTestGURLs.getGURL(JUnitTestGURLs.URL_1), 2,
            JUnitTestGURLs.getGURL(JUnitTestGURLs.URL_2), 3,
            JUnitTestGURLs.getGURL(JUnitTestGURLs.URL_3), 4,
            JUnitTestGURLs.getGURL(JUnitTestGURLs.NTP_URL), 5,
            JUnitTestGURLs.getGURL(JUnitTestGURLs.ABOUT_BLANK));

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mContext = RuntimeEnvironment.application;
        mAction = (TabSelectionEditorShareAction) TabSelectionEditorShareAction.createAction(
                mContext, ShowMode.MENU_ONLY, ButtonType.TEXT, IconPosition.START);
        mTabModel = spy(new MockTabModel(false, new MockTabModel.MockTabModelDelegate() {
            @Override
            public Tab createTab(int id, boolean incognito) {
                MockTab tab = new MockTab(id, incognito);
                tab.setGurlOverrideForTesting(mIdUrlMap.get(id));
                return tab;
            }
        }));
        when(mTabModelSelector.getCurrentModel()).thenReturn(mTabModel);
        when(mTabModelSelector.getTabModelFilterProvider())
                .thenReturn(new TabModelFilterProvider());
        mJniMocker.mock(DomDistillerUrlUtilsJni.TEST_HOOKS, mDomDistillerUrlUtilsJni);
        mAction.configure(mTabModelSelector, mSelectionDelegate, mDelegate, false);
    }

    @After
    public void tearDown() {
        TabSelectionEditorShareAction.setIntentCallbackForTesting(null);
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
        mAction.setSkipUrlCheckForTesting(true);
        List<Integer> tabIds = new ArrayList<>();
        tabIds.add(1);

        List<Tab> tabs = new ArrayList<>();
        for (int id : tabIds) {
            tabs.add(mTabModel.addTab(id));
        }

        Set<Integer> tabIdsSet = new LinkedHashSet<>(tabIds);
        when(mSelectionDelegate.getSelectedItems()).thenReturn(tabIdsSet);
        when(mDomDistillerUrlUtilsJni.getOriginalUrlFromDistillerUrl(any(String.class)))
                .thenReturn(JUnitTestGURLs.getGURL(JUnitTestGURLs.URL_1));

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

        final CallbackHelper helper = new CallbackHelper();
        ActionObserver observer = new ActionObserver() {
            @Override
            public void preProcessSelectedTabs(List<Tab> tabs) {
                helper.notifyCalled();
            }
        };
        mAction.addActionObserver(observer);

        TabSelectionEditorShareAction.setIntentCallbackForTesting((result -> {
            Assert.assertEquals(Intent.ACTION_SEND, result.getAction());
            Assert.assertEquals(
                    shareParams.getTextAndUrl(), result.getStringExtra(Intent.EXTRA_TEXT));
            Assert.assertEquals("text/plain", result.getType());
            Assert.assertEquals("1 link from Chrome", result.getStringExtra(Intent.EXTRA_TITLE));
            Assert.assertNotNull(result.getClipData());
        }));

        Assert.assertTrue(mAction.perform());

        helper.waitForFirst();
        mAction.removeActionObserver(observer);

        Assert.assertTrue(mAction.perform());
        Assert.assertEquals(1, helper.getCallCount());

        mAction.setSkipUrlCheckForTesting(false);
    }

    @Test
    @SmallTest
    public void testShareActionWithMultipleTabs() throws Exception {
        mAction.setSkipUrlCheckForTesting(true);
        List<Integer> tabIds = new ArrayList<>();
        tabIds.add(1);
        tabIds.add(2);
        tabIds.add(3);

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

        ShareParams shareParams =
                new ShareParams.Builder(tabs.get(0).getWindowAndroid(), "", "")
                        .setText(
                                "1. https://www.one.com/\n2. https://www.two.com/\n3. https://www.three.com/\n")
                        .build();

        final CallbackHelper helper = new CallbackHelper();
        ActionObserver observer = new ActionObserver() {
            @Override
            public void preProcessSelectedTabs(List<Tab> tabs) {
                helper.notifyCalled();
            }
        };
        mAction.addActionObserver(observer);

        TabSelectionEditorShareAction.setIntentCallbackForTesting((result -> {
            Assert.assertEquals(Intent.ACTION_SEND, result.getAction());
            Assert.assertEquals(
                    shareParams.getTextAndUrl(), result.getStringExtra(Intent.EXTRA_TEXT));
            Assert.assertEquals("text/plain", result.getType());
            Assert.assertEquals("3 links from Chrome", result.getStringExtra(Intent.EXTRA_TITLE));
            Assert.assertNotNull(result.getClipData());
        }));

        Assert.assertTrue(mAction.perform());

        helper.waitForFirst();
        mAction.removeActionObserver(observer);

        Assert.assertTrue(mAction.perform());
        Assert.assertEquals(1, helper.getCallCount());

        mAction.setSkipUrlCheckForTesting(false);
    }

    @Test
    @SmallTest
    public void testShareActionWithAllFilterableTabs() throws Exception {
        List<Integer> tabIds = new ArrayList<>();
        tabIds.add(4);
        tabIds.add(5);

        for (int id : tabIds) {
            mTabModel.addTab(id);
        }
        Set<Integer> tabIdsSet = new LinkedHashSet<>(tabIds);
        when(mSelectionDelegate.getSelectedItems()).thenReturn(tabIdsSet);

        mAction.onSelectionStateChange(tabIds);
        Assert.assertEquals(
                false, mAction.getPropertyModel().get(TabSelectionEditorActionProperties.ENABLED));
        Assert.assertEquals(
                2, mAction.getPropertyModel().get(TabSelectionEditorActionProperties.ITEM_COUNT));
    }
}
