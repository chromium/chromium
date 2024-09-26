// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.when;

import android.content.Context;
import android.content.Intent;
import android.graphics.drawable.Drawable;

import androidx.appcompat.content.res.AppCompatResources;
import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.MockTab;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tasks.tab_groups.TabGroupModelFilter;
import org.chromium.chrome.browser.tasks.tab_management.TabListEditorAction.ActionDelegate;
import org.chromium.chrome.browser.tasks.tab_management.TabListEditorAction.ActionObserver;
import org.chromium.chrome.browser.tasks.tab_management.TabListEditorAction.ButtonType;
import org.chromium.chrome.browser.tasks.tab_management.TabListEditorAction.IconPosition;
import org.chromium.chrome.browser.tasks.tab_management.TabListEditorAction.ShowMode;
import org.chromium.chrome.tab_ui.R;
import org.chromium.chrome.test.util.browser.tabmodel.MockTabModel;
import org.chromium.components.browser_ui.share.ShareParams;
import org.chromium.components.browser_ui.widget.selectable_list.SelectionDelegate;
import org.chromium.components.dom_distiller.core.DomDistillerUrlUtilsJni;
import org.chromium.url.GURL;
import org.chromium.url.JUnitTestGURLs;

import java.util.ArrayList;
import java.util.Collections;
import java.util.LinkedHashSet;
import java.util.List;
import java.util.Map;
import java.util.Set;

/** Unit tests for {@link TabListEditorShareAction}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class TabListEditorShareActionUnitTest {
    @Rule public JniMocker mJniMocker = new JniMocker();

    @Mock private TabGroupModelFilter mTabModelFilter;
    @Mock private SelectionDelegate<Integer> mSelectionDelegate;
    @Mock private ActionDelegate mDelegate;
    @Mock private DomDistillerUrlUtilsJni mDomDistillerUrlUtilsJni;
    @Mock private Profile mProfile;
    @Mock private Profile mIncognitoProfile;
    private Context mContext;
    private MockTabModel mTabModel;
    private TabListEditorShareAction mAction;

    Map<Integer, GURL> mIdUrlMap =
            Map.of(
                    1,
                    JUnitTestGURLs.URL_1,
                    2,
                    JUnitTestGURLs.URL_2,
                    3,
                    JUnitTestGURLs.URL_3,
                    4,
                    JUnitTestGURLs.NTP_URL,
                    5,
                    JUnitTestGURLs.ABOUT_BLANK);

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mContext = RuntimeEnvironment.application;
        mAction =
                (TabListEditorShareAction)
                        TabListEditorShareAction.createAction(
                                mContext, ShowMode.MENU_ONLY, ButtonType.TEXT, IconPosition.START);
        mTabModel =
                spy(
                        new MockTabModel(
                                mProfile,
                                new MockTabModel.MockTabModelDelegate() {
                                    @Override
                                    public MockTab createTab(int id, boolean incognito) {
                                        Profile profile = incognito ? mIncognitoProfile : mProfile;
                                        MockTab tab = new MockTab(id, profile);
                                        tab.setGurlOverrideForTesting(mIdUrlMap.get(id));
                                        return tab;
                                    }
                                }));
        when(mIncognitoProfile.isOffTheRecord()).thenReturn(true);
        when(mTabModelFilter.getTabModel()).thenReturn(mTabModel);
        doAnswer(
                        invocation -> {
                            return Collections.singletonList(
                                    mTabModel.getTabById(invocation.getArgument(0)));
                        })
                .when(mTabModelFilter)
                .getRelatedTabList(anyInt());
        mJniMocker.mock(DomDistillerUrlUtilsJni.TEST_HOOKS, mDomDistillerUrlUtilsJni);
    }

    @Test
    @SmallTest
    public void testInherentActionProperties() {
        mAction.configure(() -> mTabModelFilter, mSelectionDelegate, mDelegate, false);

        Drawable drawable =
                AppCompatResources.getDrawable(
                        mContext, R.drawable.tab_list_editor_share_icon);
        drawable.setBounds(0, 0, drawable.getIntrinsicWidth(), drawable.getIntrinsicHeight());

        Assert.assertEquals(
                R.id.tab_list_editor_share_menu_item,
                mAction.getPropertyModel().get(TabListEditorActionProperties.MENU_ITEM_ID));
        Assert.assertEquals(
                R.plurals.tab_selection_editor_share_tabs_action_button,
                mAction.getPropertyModel()
                        .get(TabListEditorActionProperties.TITLE_RESOURCE_ID));
        Assert.assertEquals(
                true,
                mAction.getPropertyModel().get(TabListEditorActionProperties.TITLE_IS_PLURAL));
        Assert.assertEquals(
                R.plurals.accessibility_tab_selection_editor_share_tabs_action_button,
                mAction.getPropertyModel()
                        .get(TabListEditorActionProperties.CONTENT_DESCRIPTION_RESOURCE_ID)
                        .intValue());
        Assert.assertNotNull(
                mAction.getPropertyModel().get(TabListEditorActionProperties.ICON));
    }

    @Test
    @SmallTest
    public void testShareActionNoTabs() {
        mAction.configure(() -> mTabModelFilter, mSelectionDelegate, mDelegate, false);

        mAction.onSelectionStateChange(new ArrayList<Integer>());
        Assert.assertEquals(
                false, mAction.getPropertyModel().get(TabListEditorActionProperties.ENABLED));
        Assert.assertEquals(
                0, mAction.getPropertyModel().get(TabListEditorActionProperties.ITEM_COUNT));
    }

    @Test
    @SmallTest
    public void testShareActionWithOneTab() throws Exception {
        mAction.configure(() -> mTabModelFilter, mSelectionDelegate, mDelegate, false);

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
                .thenReturn(JUnitTestGURLs.URL_1);

        mAction.onSelectionStateChange(tabIds);
        Assert.assertEquals(
                true, mAction.getPropertyModel().get(TabListEditorActionProperties.ENABLED));
        Assert.assertEquals(
                1, mAction.getPropertyModel().get(TabListEditorActionProperties.ITEM_COUNT));

        ShareParams shareParams =
                new ShareParams.Builder(
                                tabs.get(0).getWindowAndroid(),
                                tabs.get(0).getTitle(),
                                tabs.get(0).getUrl().getSpec())
                        .setText("")
                        .build();

        final CallbackHelper helper = new CallbackHelper();
        ActionObserver observer =
                new ActionObserver() {
                    @Override
                    public void preProcessSelectedTabs(List<Tab> tabs) {
                        helper.notifyCalled();
                    }
                };
        mAction.addActionObserver(observer);

        TabListEditorShareAction.setIntentCallbackForTesting(
                result -> {
                    Assert.assertEquals(Intent.ACTION_SEND, result.getAction());
                    Assert.assertEquals(
                            shareParams.getTextAndUrl(), result.getStringExtra(Intent.EXTRA_TEXT));
                    Assert.assertEquals("text/plain", result.getType());
                    Assert.assertEquals(
                            "1 link from Chrome", result.getStringExtra(Intent.EXTRA_TITLE));
                    Assert.assertNotNull(result.getClipData());
                });

        Assert.assertTrue(mAction.perform());

        helper.waitForOnly();
        mAction.removeActionObserver(observer);

        Assert.assertTrue(mAction.perform());
        Assert.assertEquals(1, helper.getCallCount());

        mAction.setSkipUrlCheckForTesting(false);
    }

    @Test
    @SmallTest
    public void testShareActionWithMultipleTabs() throws Exception {
        mAction.configure(() -> mTabModelFilter, mSelectionDelegate, mDelegate, false);

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
                true, mAction.getPropertyModel().get(TabListEditorActionProperties.ENABLED));
        Assert.assertEquals(
                3, mAction.getPropertyModel().get(TabListEditorActionProperties.ITEM_COUNT));

        ShareParams shareParams =
                new ShareParams.Builder(tabs.get(0).getWindowAndroid(), "", "")
                        .setText(
                                "1. https://www.one.com/\n"
                                        + "2. https://www.two.com/\n"
                                        + "3. https://www.three.com/\n")
                        .build();

        final CallbackHelper helper = new CallbackHelper();
        ActionObserver observer =
                new ActionObserver() {
                    @Override
                    public void preProcessSelectedTabs(List<Tab> tabs) {
                        helper.notifyCalled();
                    }
                };
        mAction.addActionObserver(observer);

        TabListEditorShareAction.setIntentCallbackForTesting(
                result -> {
                    Assert.assertEquals(Intent.ACTION_SEND, result.getAction());
                    Assert.assertEquals(
                            shareParams.getTextAndUrl(), result.getStringExtra(Intent.EXTRA_TEXT));
                    Assert.assertEquals("text/plain", result.getType());
                    Assert.assertEquals(
                            "3 links from Chrome", result.getStringExtra(Intent.EXTRA_TITLE));
                    Assert.assertNotNull(result.getClipData());
                });

        Assert.assertTrue(mAction.perform());

        helper.waitForOnly();
        mAction.removeActionObserver(observer);

        Assert.assertTrue(mAction.perform());
        Assert.assertEquals(1, helper.getCallCount());

        mAction.setSkipUrlCheckForTesting(false);
    }

    @Test
    @SmallTest
    public void testShareActionWithAllFilterableTabs_actionsOnTabs() throws Exception {
        mAction.configure(() -> mTabModelFilter, mSelectionDelegate, mDelegate, false);

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
                false, mAction.getPropertyModel().get(TabListEditorActionProperties.ENABLED));
        Assert.assertEquals(
                2, mAction.getPropertyModel().get(TabListEditorActionProperties.ITEM_COUNT));
    }

    @Test
    @SmallTest
    public void testShareActionWithAllFilterableTabs_actionsOnTabsAndRelatedTabs()
            throws Exception {
        mAction.configure(() -> mTabModelFilter, mSelectionDelegate, mDelegate, true);

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
                false, mAction.getPropertyModel().get(TabListEditorActionProperties.ENABLED));
        Assert.assertEquals(
                2, mAction.getPropertyModel().get(TabListEditorActionProperties.ITEM_COUNT));
    }
}
