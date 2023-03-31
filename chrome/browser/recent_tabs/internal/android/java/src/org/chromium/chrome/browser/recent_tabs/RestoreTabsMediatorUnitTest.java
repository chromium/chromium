// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.recent_tabs;

import static org.hamcrest.Matchers.instanceOf;

import static org.chromium.chrome.browser.recent_tabs.RestoreTabsProperties.CURRENT_SCREEN;
import static org.chromium.chrome.browser.recent_tabs.RestoreTabsProperties.DETAIL_SCREEN_BACK_CLICK_HANDLER;
import static org.chromium.chrome.browser.recent_tabs.RestoreTabsProperties.DETAIL_SCREEN_MODEL_LIST;
import static org.chromium.chrome.browser.recent_tabs.RestoreTabsProperties.DEVICE_MODEL_LIST;
import static org.chromium.chrome.browser.recent_tabs.RestoreTabsProperties.HOME_SCREEN_DELEGATE;
import static org.chromium.chrome.browser.recent_tabs.RestoreTabsProperties.NUM_TABS_DESELECTED;
import static org.chromium.chrome.browser.recent_tabs.RestoreTabsProperties.REVIEW_TABS_MODEL_LIST;
import static org.chromium.chrome.browser.recent_tabs.RestoreTabsProperties.REVIEW_TABS_SCREEN_DELEGATE;
import static org.chromium.chrome.browser.recent_tabs.RestoreTabsProperties.SELECTED_DEVICE;
import static org.chromium.chrome.browser.recent_tabs.RestoreTabsProperties.ScreenType.DEVICE_SCREEN;
import static org.chromium.chrome.browser.recent_tabs.RestoreTabsProperties.ScreenType.HOME_SCREEN;
import static org.chromium.chrome.browser.recent_tabs.RestoreTabsProperties.ScreenType.REVIEW_TABS_SCREEN;
import static org.chromium.chrome.browser.recent_tabs.RestoreTabsProperties.VISIBLE;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.recent_tabs.ForeignSessionHelper.ForeignSession;
import org.chromium.chrome.browser.recent_tabs.ForeignSessionHelper.ForeignSessionTab;
import org.chromium.chrome.browser.recent_tabs.ForeignSessionHelper.ForeignSessionWindow;
import org.chromium.chrome.browser.recent_tabs.RestoreTabsProperties.DetailItemType;
import org.chromium.chrome.browser.recent_tabs.ui.RestoreTabsDetailScreenCoordinator;
import org.chromium.chrome.browser.recent_tabs.ui.RestoreTabsPromoScreenCoordinator;
import org.chromium.chrome.browser.recent_tabs.ui.TabItemProperties;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.url.JUnitTestGURLs;

import java.util.ArrayList;
import java.util.List;

/** Tests for RestoreTabsMediator. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class RestoreTabsMediatorUnitTest {
    private PropertyModel mModel = RestoreTabsProperties.createDefaultModel();
    private RestoreTabsMediator mMediator = new RestoreTabsMediator();

    @Before
    public void setUp() {
        mMediator.initialize(mModel, new RestoreTabsControllerFactory.ControllerListener() {
            @Override
            public void onDismissed() {
                mMediator.destroy();
            }
        });
    }

    @After
    public void tearDown() {
        mModel = null;
    }

    @Test
    public void testRestoreTabsMediator_initCreatesValidDefaultModel() {
        Assert.assertEquals(mModel.get(VISIBLE), false);
        Assert.assertNotNull(mModel.get(HOME_SCREEN_DELEGATE));
        Assert.assertThat(mModel.get(HOME_SCREEN_DELEGATE),
                instanceOf(RestoreTabsPromoScreenCoordinator.Delegate.class));
        Assert.assertNotNull(mModel.get(DETAIL_SCREEN_BACK_CLICK_HANDLER));
        Assert.assertEquals(mModel.get(CURRENT_SCREEN), HOME_SCREEN);
        Assert.assertEquals(mModel.get(NUM_TABS_DESELECTED), 0);
    }

    @Test
    public void testRestoreTabsMediator_onDismissed() {
        ForeignSession session =
                new ForeignSession("tag", "John's iPhone 6", 32L, new ArrayList<>());
        List<ForeignSession> testSessions = new ArrayList<>();
        testSessions.add(session);

        mMediator.showOptions(testSessions);
        Assert.assertEquals(mModel.get(VISIBLE), true);
        mMediator.dismiss();
        Assert.assertEquals(mModel.get(VISIBLE), false);
    }

    @Test
    public void testRestoreTabsMediator_showOptionsUpdatesModel() {
        ForeignSession session =
                new ForeignSession("tag", "John's iPhone 6", 32L, new ArrayList<>());
        List<ForeignSession> testSessions = new ArrayList<>();
        testSessions.add(session);

        mMediator.showOptions(testSessions);

        Assert.assertEquals(mModel.get(VISIBLE), true);
        Assert.assertEquals(mModel.get(CURRENT_SCREEN), HOME_SCREEN);
        Assert.assertEquals(mModel.get(SELECTED_DEVICE), testSessions.get(0));
    }

    @Test
    public void testRestoreTabsMediator_createHomeScreenDelegate() {
        RestoreTabsPromoScreenCoordinator.Delegate delegate = mModel.get(HOME_SCREEN_DELEGATE);

        // Testing the onShowDeviceList member function.
        delegate.onShowDeviceList();
        Assert.assertEquals(mModel.get(CURRENT_SCREEN), DEVICE_SCREEN);

        // Testing the onAllTabsChosen member function.
        delegate.onAllTabsChosen();
        Assert.assertEquals(mModel.get(VISIBLE), false);

        // Testing the onReviewTabsChosen member function.
        delegate.onReviewTabsChosen();
        Assert.assertEquals(mModel.get(CURRENT_SCREEN), REVIEW_TABS_SCREEN);
    }

    @Test
    public void testRestoreTabsMediator_setDeviceListItemsNoSelection() {
        ForeignSession session1 =
                new ForeignSession("tag1", "John's iPhone 6", 32L, new ArrayList<>());
        ForeignSession session2 =
                new ForeignSession("tag2", "John's iPhone 7", 33L, new ArrayList<>());
        List<ForeignSession> testSessions = new ArrayList<>();
        testSessions.add(session1);
        testSessions.add(session2);

        mMediator.setDeviceListItems(testSessions);

        Assert.assertEquals(mModel.get(SELECTED_DEVICE), testSessions.get(0));
    }

    @Test
    public void testRestoreTabsMediator_setDeviceListItemsSelection() {
        ForeignSession session1 =
                new ForeignSession("tag1", "John's iPhone 6", 32L, new ArrayList<>());
        ForeignSession session2 =
                new ForeignSession("tag2", "John's iPhone 7", 33L, new ArrayList<>());
        List<ForeignSession> testSessions = new ArrayList<>();
        testSessions.add(session1);
        testSessions.add(session2);

        mModel.set(SELECTED_DEVICE, session2);

        mMediator.setDeviceListItems(testSessions);

        Assert.assertEquals(mModel.get(SELECTED_DEVICE), testSessions.get(1));
    }

    @Test
    public void testRestoreTabsMediator_setSelectedDeviceItem() {
        ForeignSession session =
                new ForeignSession("tag", "John's iPhone 6", 32L, new ArrayList<>());
        mMediator.setSelectedDeviceItem(session);

        Assert.assertEquals(mModel.get(SELECTED_DEVICE), session);
    }

    @Test
    public void testRestoreTabsMediator_setCurrentScreenDevices() {
        mMediator.setCurrentScreen(DEVICE_SCREEN);

        Assert.assertEquals(mModel.get(DETAIL_SCREEN_MODEL_LIST), mModel.get(DEVICE_MODEL_LIST));
        Assert.assertNull(mModel.get(REVIEW_TABS_SCREEN_DELEGATE));
        Assert.assertEquals(mModel.get(CURRENT_SCREEN), DEVICE_SCREEN);
    }

    @Test
    public void testRestoreTabsMediator_setCurrentScreenReviewTabs() {
        mMediator.setCurrentScreen(REVIEW_TABS_SCREEN);

        Assert.assertEquals(
                mModel.get(DETAIL_SCREEN_MODEL_LIST), mModel.get(REVIEW_TABS_MODEL_LIST));
        Assert.assertNotNull(mModel.get(REVIEW_TABS_SCREEN_DELEGATE));
        Assert.assertThat(mModel.get(REVIEW_TABS_SCREEN_DELEGATE),
                instanceOf(RestoreTabsDetailScreenCoordinator.Delegate.class));
        Assert.assertEquals(mModel.get(CURRENT_SCREEN), REVIEW_TABS_SCREEN);
    }

    @Test
    public void testRestoreTabsMediator_setCurrentScreenHome() {
        mMediator.setCurrentScreen(HOME_SCREEN);

        Assert.assertNull(mModel.get(DETAIL_SCREEN_MODEL_LIST));
        Assert.assertEquals(mModel.get(CURRENT_SCREEN), HOME_SCREEN);
    }

    @Test
    public void testRestoreTabsMediator_setTabListItems() {
        ForeignSessionTab tab = new ForeignSessionTab(
                JUnitTestGURLs.getGURL(JUnitTestGURLs.URL_1), "title", 32L, 0);
        List<ForeignSessionTab> tabs = new ArrayList<>();
        tabs.add(tab);

        ForeignSessionWindow window = new ForeignSessionWindow(31L, 1, tabs);
        List<ForeignSessionWindow> windows = new ArrayList<>();
        windows.add(window);

        ForeignSession session = new ForeignSession("tag", "John's iPhone 6", 32L, windows);
        mModel.set(SELECTED_DEVICE, session);
        mMediator.setTabListItems();

        ModelList tabItems = mModel.get(REVIEW_TABS_MODEL_LIST);
        Assert.assertEquals(tabItems.size(), 1);
    }

    @Test
    public void testRestoreTabsMediator_toggleTabSelectedStateTrue() {
        ForeignSessionTab tab = new ForeignSessionTab(
                JUnitTestGURLs.getGURL(JUnitTestGURLs.URL_1), "title", 32L, 0);
        PropertyModel model = TabItemProperties.create(tab, true);
        mMediator.toggleTabSelectedState(model);

        Assert.assertEquals(model.get(TabItemProperties.IS_SELECTED), false);
        Assert.assertEquals(mModel.get(NUM_TABS_DESELECTED), 1);
    }

    @Test
    public void testRestoreTabsMediator_toggleTabSelectedStateFalse() {
        ForeignSessionTab tab = new ForeignSessionTab(
                JUnitTestGURLs.getGURL(JUnitTestGURLs.URL_1), "title", 32L, 0);
        PropertyModel model = TabItemProperties.create(tab, false);
        mMediator.toggleTabSelectedState(model);

        Assert.assertEquals(model.get(TabItemProperties.IS_SELECTED), true);
        Assert.assertEquals(mModel.get(NUM_TABS_DESELECTED), -1);
    }

    @Test
    public void testRestoreTabsMediator_createReviewTabsScreenDelegate() {
        mMediator.setCurrentScreen(REVIEW_TABS_SCREEN);
        RestoreTabsDetailScreenCoordinator.Delegate delegate =
                mModel.get(REVIEW_TABS_SCREEN_DELEGATE);

        // Testing the onChangeSelectionStateForAllTabs function with a deselected tab.
        mModel.set(NUM_TABS_DESELECTED, 1);
        delegate.onChangeSelectionStateForAllTabs();
        Assert.assertEquals(mModel.get(NUM_TABS_DESELECTED), 0);

        // Testing the onChangeSelectionStateForAllTabs function with no deselected tabs.
        ForeignSessionTab tab = new ForeignSessionTab(
                JUnitTestGURLs.getGURL(JUnitTestGURLs.URL_1), "title", 32L, 0);
        ModelList tabItems = mModel.get(REVIEW_TABS_MODEL_LIST);
        PropertyModel model = TabItemProperties.create(/*tab=*/tab, /*isSelected=*/true);
        tabItems.add(new ListItem(DetailItemType.TAB, model));
        delegate.onChangeSelectionStateForAllTabs();
        Assert.assertEquals(mModel.get(NUM_TABS_DESELECTED), tabItems.size());

        // Testing the onSelectedTabsChosen member function.
        delegate.onSelectedTabsChosen();
        Assert.assertEquals(mModel.get(VISIBLE), false);
    }

    @Test
    public void testRestoreTabsMediator_toggleTabSelectedStateAllTabsTrueWithDelegate() {
        mMediator.setCurrentScreen(REVIEW_TABS_SCREEN);
        ForeignSessionTab tab1 = new ForeignSessionTab(
                JUnitTestGURLs.getGURL(JUnitTestGURLs.URL_1), "title", 32L, 0);
        PropertyModel model1 = TabItemProperties.create(tab1, true);

        ForeignSessionTab tab2 = new ForeignSessionTab(
                JUnitTestGURLs.getGURL(JUnitTestGURLs.URL_1), "title", 32L, 0);
        PropertyModel model2 = TabItemProperties.create(tab2, true);

        ForeignSessionTab tab3 = new ForeignSessionTab(
                JUnitTestGURLs.getGURL(JUnitTestGURLs.URL_1), "title", 32L, 0);
        PropertyModel model3 = TabItemProperties.create(tab3, true);

        ModelList tabItems = mModel.get(REVIEW_TABS_MODEL_LIST);
        tabItems.add(new ListItem(DetailItemType.TAB, model1));
        tabItems.add(new ListItem(DetailItemType.TAB, model2));
        tabItems.add(new ListItem(DetailItemType.TAB, model3));

        mMediator.toggleTabSelectedState(model1);
        mMediator.toggleTabSelectedState(model2);
        mMediator.toggleTabSelectedState(model3);

        Assert.assertEquals(mModel.get(NUM_TABS_DESELECTED), 3);

        RestoreTabsDetailScreenCoordinator.Delegate delegate =
                mModel.get(REVIEW_TABS_SCREEN_DELEGATE);

        delegate.onChangeSelectionStateForAllTabs();
        Assert.assertEquals(mModel.get(NUM_TABS_DESELECTED), 0);
    }

    @Test
    public void testRestoreTabsMediator_toggleTabSelectedStateAllTabsFalseWithDelegate() {
        mMediator.setCurrentScreen(REVIEW_TABS_SCREEN);
        ForeignSessionTab tab1 = new ForeignSessionTab(
                JUnitTestGURLs.getGURL(JUnitTestGURLs.URL_1), "title", 32L, 0);
        PropertyModel model1 = TabItemProperties.create(tab1, false);

        ForeignSessionTab tab2 = new ForeignSessionTab(
                JUnitTestGURLs.getGURL(JUnitTestGURLs.URL_1), "title", 32L, 0);
        PropertyModel model2 = TabItemProperties.create(tab2, false);

        ForeignSessionTab tab3 = new ForeignSessionTab(
                JUnitTestGURLs.getGURL(JUnitTestGURLs.URL_1), "title", 32L, 0);
        PropertyModel model3 = TabItemProperties.create(tab3, false);

        mModel.set(NUM_TABS_DESELECTED, 3);
        ModelList tabItems = mModel.get(REVIEW_TABS_MODEL_LIST);
        tabItems.add(new ListItem(DetailItemType.TAB, model1));
        tabItems.add(new ListItem(DetailItemType.TAB, model2));
        tabItems.add(new ListItem(DetailItemType.TAB, model3));

        mMediator.toggleTabSelectedState(model1);
        mMediator.toggleTabSelectedState(model2);
        mMediator.toggleTabSelectedState(model3);

        Assert.assertEquals(mModel.get(NUM_TABS_DESELECTED), 0);

        RestoreTabsDetailScreenCoordinator.Delegate delegate =
                mModel.get(REVIEW_TABS_SCREEN_DELEGATE);

        delegate.onChangeSelectionStateForAllTabs();
        Assert.assertEquals(mModel.get(NUM_TABS_DESELECTED), tabItems.size());
    }

    @Test
    public void testRestoreTabsMediator_toggleTabSelectedStateAllTabsMixedWithDelegate() {
        mMediator.setCurrentScreen(REVIEW_TABS_SCREEN);
        ForeignSessionTab tab1 = new ForeignSessionTab(
                JUnitTestGURLs.getGURL(JUnitTestGURLs.URL_1), "title", 32L, 0);
        PropertyModel model1 = TabItemProperties.create(tab1, true);

        ForeignSessionTab tab2 = new ForeignSessionTab(
                JUnitTestGURLs.getGURL(JUnitTestGURLs.URL_1), "title", 32L, 0);
        PropertyModel model2 = TabItemProperties.create(tab2, false);

        ForeignSessionTab tab3 = new ForeignSessionTab(
                JUnitTestGURLs.getGURL(JUnitTestGURLs.URL_1), "title", 32L, 0);
        PropertyModel model3 = TabItemProperties.create(tab3, true);

        mModel.set(NUM_TABS_DESELECTED, 1);
        ModelList tabItems = mModel.get(REVIEW_TABS_MODEL_LIST);
        tabItems.add(new ListItem(DetailItemType.TAB, model1));
        tabItems.add(new ListItem(DetailItemType.TAB, model2));
        tabItems.add(new ListItem(DetailItemType.TAB, model3));

        mMediator.toggleTabSelectedState(model1);
        mMediator.toggleTabSelectedState(model2);
        mMediator.toggleTabSelectedState(model3);

        Assert.assertEquals(mModel.get(NUM_TABS_DESELECTED), 2);

        RestoreTabsDetailScreenCoordinator.Delegate delegate =
                mModel.get(REVIEW_TABS_SCREEN_DELEGATE);

        delegate.onChangeSelectionStateForAllTabs();
        Assert.assertEquals(mModel.get(NUM_TABS_DESELECTED), 0);
    }

    @Test
    public void testRestoreTabsMediator_toggleTabSelectedStateSubsetTabsWithDelegate() {
        mMediator.setCurrentScreen(REVIEW_TABS_SCREEN);
        ForeignSessionTab tab1 = new ForeignSessionTab(
                JUnitTestGURLs.getGURL(JUnitTestGURLs.URL_1), "title", 32L, 0);
        PropertyModel model1 = TabItemProperties.create(tab1, true);

        ForeignSessionTab tab2 = new ForeignSessionTab(
                JUnitTestGURLs.getGURL(JUnitTestGURLs.URL_1), "title", 32L, 0);
        PropertyModel model2 = TabItemProperties.create(tab2, true);

        ForeignSessionTab tab3 = new ForeignSessionTab(
                JUnitTestGURLs.getGURL(JUnitTestGURLs.URL_1), "title", 32L, 0);
        PropertyModel model3 = TabItemProperties.create(tab3, true);

        ModelList tabItems = mModel.get(REVIEW_TABS_MODEL_LIST);
        tabItems.add(new ListItem(DetailItemType.TAB, model1));
        tabItems.add(new ListItem(DetailItemType.TAB, model2));
        tabItems.add(new ListItem(DetailItemType.TAB, model3));

        mMediator.toggleTabSelectedState(model1);
        mMediator.toggleTabSelectedState(model3);

        Assert.assertEquals(mModel.get(NUM_TABS_DESELECTED), 2);

        RestoreTabsDetailScreenCoordinator.Delegate delegate =
                mModel.get(REVIEW_TABS_SCREEN_DELEGATE);

        delegate.onChangeSelectionStateForAllTabs();
        Assert.assertEquals(mModel.get(NUM_TABS_DESELECTED), 0);
    }
}
