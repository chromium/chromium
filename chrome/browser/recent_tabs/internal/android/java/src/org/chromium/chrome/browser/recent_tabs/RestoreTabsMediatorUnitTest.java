// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.recent_tabs;

import static androidx.test.espresso.matcher.ViewMatchers.assertThat;

import static org.hamcrest.Matchers.instanceOf;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import static org.chromium.chrome.browser.recent_tabs.RestoreTabsProperties.CURRENT_SCREEN;
import static org.chromium.chrome.browser.recent_tabs.RestoreTabsProperties.DETAIL_SCREEN_BACK_CLICK_HANDLER;
import static org.chromium.chrome.browser.recent_tabs.RestoreTabsProperties.DETAIL_SCREEN_MODEL_LIST;
import static org.chromium.chrome.browser.recent_tabs.RestoreTabsProperties.DETAIL_SCREEN_TITLE;
import static org.chromium.chrome.browser.recent_tabs.RestoreTabsProperties.DEVICE_MODEL_LIST;
import static org.chromium.chrome.browser.recent_tabs.RestoreTabsProperties.HOME_SCREEN_DELEGATE;
import static org.chromium.chrome.browser.recent_tabs.RestoreTabsProperties.NUM_TABS_DESELECTED;
import static org.chromium.chrome.browser.recent_tabs.RestoreTabsProperties.REVIEW_TABS_MODEL_LIST;
import static org.chromium.chrome.browser.recent_tabs.RestoreTabsProperties.REVIEW_TABS_SCREEN_DELEGATE;
import static org.chromium.chrome.browser.recent_tabs.RestoreTabsProperties.SELECTED_DEVICE;
import static org.chromium.chrome.browser.recent_tabs.RestoreTabsProperties.ScreenType.DEVICE_SCREEN;
import static org.chromium.chrome.browser.recent_tabs.RestoreTabsProperties.ScreenType.HOME_SCREEN;
import static org.chromium.chrome.browser.recent_tabs.RestoreTabsProperties.ScreenType.REVIEW_TABS_SCREEN;
import static org.chromium.chrome.browser.recent_tabs.RestoreTabsProperties.ScreenType.UNINITIALIZED;
import static org.chromium.chrome.browser.recent_tabs.RestoreTabsProperties.VISIBLE;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.recent_tabs.ForeignSessionHelper.ForeignSession;
import org.chromium.chrome.browser.recent_tabs.ForeignSessionHelper.ForeignSessionTab;
import org.chromium.chrome.browser.recent_tabs.ForeignSessionHelper.ForeignSessionWindow;
import org.chromium.chrome.browser.recent_tabs.RestoreTabsProperties.DetailItemType;
import org.chromium.chrome.browser.recent_tabs.ui.RestoreTabsDetailScreenCoordinator;
import org.chromium.chrome.browser.recent_tabs.ui.RestoreTabsPromoScreenCoordinator;
import org.chromium.chrome.browser.recent_tabs.ui.TabItemProperties;
import org.chromium.chrome.browser.tabmodel.TabCreatorManager;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetObserver;
import org.chromium.components.feature_engagement.EventConstants;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.components.sync_device_info.FormFactor;
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
    private static final String RESTORE_TABS_USED = EventConstants.RESTORE_TABS_PROMO_USED;

    @Mock private RestoreTabsControllerDelegate mDelegate;
    @Mock private ForeignSessionHelper mForeignSessionHelper;
    @Mock private TabCreatorManager mTabCreatorManager;
    @Mock private BottomSheetController mBottomSheetController;
    @Mock private Profile mProfile;
    @Mock private Tracker mTracker;
    @Mock private BottomSheetContent mBottomSheetContent;

    private PropertyModel mModel = RestoreTabsProperties.createDefaultModel();
    private RestoreTabsMediator mMediator = new RestoreTabsMediator();

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        TrackerFactory.setTrackerForTests(mTracker);
        mMediator.initialize(mModel, mProfile, mTabCreatorManager, mBottomSheetController);
    }

    @After
    public void tearDown() {
        mMediator.destroy();
        TrackerFactory.setTrackerForTests(null);
        mModel = null;
    }

    @Test
    public void testRestoreTabsMediator_initCreatesValidDefaultModel() {
        Assert.assertEquals(mModel.get(VISIBLE), false);
        Assert.assertNotNull(mModel.get(HOME_SCREEN_DELEGATE));
        assertThat(
                mModel.get(HOME_SCREEN_DELEGATE),
                instanceOf(RestoreTabsPromoScreenCoordinator.Delegate.class));
        Assert.assertNotNull(mModel.get(DETAIL_SCREEN_BACK_CLICK_HANDLER));
        Assert.assertEquals(mModel.get(CURRENT_SCREEN), UNINITIALIZED);
        Assert.assertEquals(mModel.get(NUM_TABS_DESELECTED), 0);
    }

    @Test
    public void testRestoreTabsMediator_onDismissed() {
        ForeignSession session =
                new ForeignSession(
                        "tag", "John's iPhone 6", 32L, new ArrayList<>(), FormFactor.PHONE);
        List<ForeignSession> testSessions = new ArrayList<>();
        testSessions.add(session);

        mMediator.showHomeScreen(mForeignSessionHelper, testSessions, mDelegate);
        Assert.assertEquals(mModel.get(VISIBLE), true);
        mMediator.dismiss();
        verify(mDelegate).onDismissed();
        Assert.assertEquals(mModel.get(VISIBLE), false);
    }

    @Test
    public void testRestoreTabsMediator_isVisible() {
        when(mBottomSheetController.requestShowContent(mBottomSheetContent, true)).thenReturn(true);
        Assert.assertTrue(mMediator.setVisible(true, mBottomSheetContent));
        verify(mBottomSheetController).addObserver(any(BottomSheetObserver.class));
    }

    @Test
    public void testRestoreTabsMediator_isVisibleButDidNotShow() {
        when(mBottomSheetController.requestShowContent(mBottomSheetContent, true))
                .thenReturn(false);
        Assert.assertFalse(mMediator.setVisible(true, mBottomSheetContent));
        verify(mBottomSheetController).removeObserver(any(BottomSheetObserver.class));
    }

    @Test
    public void testRestoreTabsMediator_isNotVisible() {
        Assert.assertTrue(mMediator.setVisible(false, mBottomSheetContent));
        verify(mBottomSheetController).hideContent(mBottomSheetContent, true);
    }

    @Test
    public void testRestoreTabsMediator_showOptionsUpdatesModel() {
        ForeignSession session =
                new ForeignSession(
                        "tag", "John's iPhone 6", 32L, new ArrayList<>(), FormFactor.PHONE);
        List<ForeignSession> testSessions = new ArrayList<>();
        testSessions.add(session);

        mMediator.showHomeScreen(mForeignSessionHelper, testSessions, mDelegate);
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

        // Testing the onReviewTabsChosen member function.
        delegate.onReviewTabsChosen();
        Assert.assertEquals(mModel.get(CURRENT_SCREEN), REVIEW_TABS_SCREEN);
    }

    @Test
    public void testRestoreTabsMediator_createHomeScreenDelegateOnAllTabsChosen() {
        RestoreTabsMetricsHelper.setPromoShownCount(1);
        RestoreTabsPromoScreenCoordinator.Delegate delegate = mModel.get(HOME_SCREEN_DELEGATE);
        mModel.set(VISIBLE, true);

        ForeignSessionTab tab1 = new ForeignSessionTab(JUnitTestGURLs.URL_1, "title", 32L, 32L, 0);
        ModelList tabItems = mModel.get(REVIEW_TABS_MODEL_LIST);
        PropertyModel model1 = TabItemProperties.create(/* tab= */ tab1, /* isSelected= */ true);
        tabItems.add(new ListItem(DetailItemType.TAB, model1));
        ForeignSessionTab tab2 = new ForeignSessionTab(JUnitTestGURLs.URL_1, "title", 32L, 32L, 0);
        PropertyModel model2 = TabItemProperties.create(/* tab= */ tab2, /* isSelected= */ true);
        tabItems.add(new ListItem(DetailItemType.TAB, model2));

        // Only add the selected tab
        List<ForeignSessionTab> tabs = new ArrayList<>();
        tabs.add(tab1);
        tabs.add(tab2);

        ForeignSessionWindow window = new ForeignSessionWindow(31L, 1, tabs);
        List<ForeignSessionWindow> windows = new ArrayList<>();
        windows.add(window);

        ForeignSession session =
                new ForeignSession("tag", "John's iPhone 6", 32L, windows, FormFactor.PHONE);
        mModel.set(SELECTED_DEVICE, session);
        List<ForeignSession> sessions = new ArrayList<>();
        sessions.add(session);

        mMediator.showHomeScreen(mForeignSessionHelper, sessions, mDelegate);
        delegate.onAllTabsChosen();
        verify(mDelegate).getGTSTabListModelSize();
        verify(mForeignSessionHelper)
                .openForeignSessionTabsAsBackgroundTabs(
                        tabs, mModel.get(SELECTED_DEVICE), mTabCreatorManager);
        verify(mTracker).notifyEvent(eq(RESTORE_TABS_USED));
        Assert.assertEquals(mModel.get(VISIBLE), false);
        verify(mDelegate).scrollGTSToRestoredTabs(0);
        RestoreTabsMetricsHelper.setPromoShownCount(0);
    }

    @Test
    public void testRestoreTabsMediator_setDeviceListItemsNoSelection() {
        ForeignSession session1 =
                new ForeignSession(
                        "tag1", "John's iPhone 6", 32L, new ArrayList<>(), FormFactor.PHONE);
        ForeignSession session2 =
                new ForeignSession(
                        "tag2", "John's iPhone 7", 33L, new ArrayList<>(), FormFactor.PHONE);
        List<ForeignSession> testSessions = new ArrayList<>();
        testSessions.add(session1);
        testSessions.add(session2);

        mMediator.setDeviceListItems(testSessions);

        Assert.assertEquals(mModel.get(SELECTED_DEVICE), testSessions.get(0));
    }

    @Test
    public void testRestoreTabsMediator_setDeviceListItemsSelection() {
        ForeignSession session1 =
                new ForeignSession(
                        "tag1", "John's iPhone 6", 32L, new ArrayList<>(), FormFactor.PHONE);
        ForeignSession session2 =
                new ForeignSession(
                        "tag2", "John's iPhone 7", 33L, new ArrayList<>(), FormFactor.PHONE);
        List<ForeignSession> testSessions = new ArrayList<>();
        testSessions.add(session1);
        testSessions.add(session2);

        mModel.set(SELECTED_DEVICE, session2);

        mMediator.setDeviceListItems(testSessions);

        // Resulting selected device sorted based on recent modified time.
        Assert.assertEquals(mModel.get(SELECTED_DEVICE), testSessions.get(0));
        Assert.assertEquals(testSessions.get(0).tag, session2.tag);
        Assert.assertEquals(testSessions.get(0).name, session2.name);
        Assert.assertEquals(testSessions.get(0).modifiedTime, session2.modifiedTime);
        Assert.assertEquals(testSessions.get(0).formFactor, session2.formFactor);
    }

    @Test
    public void testRestoreTabsMediator_setSelectedDeviceItem() {
        ForeignSession session =
                new ForeignSession(
                        "tag", "John's iPhone 6", 32L, new ArrayList<>(), FormFactor.PHONE);
        mMediator.setSelectedDeviceItem(session);

        Assert.assertEquals(mModel.get(SELECTED_DEVICE), session);
    }

    @Test
    public void testRestoreTabsMediator_setSelectedDeviceItemResetsTabList() {
        ForeignSessionTab tab1 = new ForeignSessionTab(JUnitTestGURLs.URL_1, "title", 32L, 32L, 0);
        PropertyModel model1 = TabItemProperties.create(tab1, false);

        ModelList tabItems = mModel.get(REVIEW_TABS_MODEL_LIST);
        tabItems.add(new ListItem(DetailItemType.TAB, model1));

        Assert.assertEquals(tabItems.size(), 1);

        // Add two new tabs to check they are not the same as the one above.
        ForeignSessionTab tab2 = new ForeignSessionTab(JUnitTestGURLs.URL_1, "title2", 32L, 32L, 0);
        ForeignSessionTab tab3 = new ForeignSessionTab(JUnitTestGURLs.URL_1, "title3", 32L, 32L, 0);
        List<ForeignSessionTab> tabs = new ArrayList<>();
        tabs.add(tab2);
        tabs.add(tab3);

        ForeignSessionWindow window = new ForeignSessionWindow(31L, 1, tabs);
        List<ForeignSessionWindow> windows = new ArrayList<>();
        windows.add(window);

        ForeignSession session =
                new ForeignSession("tag", "John's iPhone 6", 32L, windows, FormFactor.PHONE);
        mModel.set(NUM_TABS_DESELECTED, 1);
        mMediator.setSelectedDeviceItem(session);

        Assert.assertEquals(mModel.get(SELECTED_DEVICE), session);
        Assert.assertEquals(tabItems.size(), 2);
        Assert.assertEquals(tabItems.get(0).model.get(TabItemProperties.FOREIGN_SESSION_TAB), tab2);
        Assert.assertEquals(tabItems.get(1).model.get(TabItemProperties.FOREIGN_SESSION_TAB), tab3);
        Assert.assertEquals(mModel.get(NUM_TABS_DESELECTED), 0);
    }

    @Test
    public void testRestoreTabsMediator_setCurrentScreenDevices() {
        mMediator.setCurrentScreen(DEVICE_SCREEN);

        Assert.assertEquals(mModel.get(DETAIL_SCREEN_MODEL_LIST), mModel.get(DEVICE_MODEL_LIST));
        Assert.assertEquals(
                mModel.get(DETAIL_SCREEN_TITLE), R.string.restore_tabs_device_screen_sheet_title);
        Assert.assertNull(mModel.get(REVIEW_TABS_SCREEN_DELEGATE));
        Assert.assertEquals(mModel.get(CURRENT_SCREEN), DEVICE_SCREEN);
    }

    @Test
    public void testRestoreTabsMediator_setCurrentScreenReviewTabs() {
        mMediator.setCurrentScreen(REVIEW_TABS_SCREEN);

        Assert.assertEquals(
                mModel.get(DETAIL_SCREEN_MODEL_LIST), mModel.get(REVIEW_TABS_MODEL_LIST));
        Assert.assertNotNull(mModel.get(REVIEW_TABS_SCREEN_DELEGATE));
        Assert.assertEquals(
                mModel.get(DETAIL_SCREEN_TITLE),
                R.string.restore_tabs_review_tabs_screen_sheet_title);
        assertThat(
                mModel.get(REVIEW_TABS_SCREEN_DELEGATE),
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
        ForeignSessionTab tab = new ForeignSessionTab(JUnitTestGURLs.URL_1, "title", 32L, 32L, 0);
        List<ForeignSessionTab> tabs = new ArrayList<>();
        tabs.add(tab);

        ForeignSessionWindow window = new ForeignSessionWindow(31L, 1, tabs);
        List<ForeignSessionWindow> windows = new ArrayList<>();
        windows.add(window);

        ForeignSession session = new ForeignSession("tag", "John's iPhone 6", 32L, windows, 2);
        mModel.set(SELECTED_DEVICE, session);
        mMediator.setTabListItems();

        ModelList tabItems = mModel.get(REVIEW_TABS_MODEL_LIST);
        Assert.assertEquals(tabItems.size(), 1);
    }

    @Test
    public void testRestoreTabsMediator_toggleTabSelectedStateTrue() {
        ForeignSessionTab tab = new ForeignSessionTab(JUnitTestGURLs.URL_1, "title", 32L, 32L, 0);
        PropertyModel model = TabItemProperties.create(tab, true);
        mMediator.toggleTabSelectedState(model);

        Assert.assertEquals(model.get(TabItemProperties.IS_SELECTED), false);
        Assert.assertEquals(mModel.get(NUM_TABS_DESELECTED), 1);
    }

    @Test
    public void testRestoreTabsMediator_toggleTabSelectedStateFalse() {
        ForeignSessionTab tab = new ForeignSessionTab(JUnitTestGURLs.URL_1, "title", 32L, 32L, 0);
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
        ForeignSessionTab tab = new ForeignSessionTab(JUnitTestGURLs.URL_1, "title", 32L, 32L, 0);
        ModelList tabItems = mModel.get(REVIEW_TABS_MODEL_LIST);
        PropertyModel model = TabItemProperties.create(/* tab= */ tab, /* isSelected= */ true);
        tabItems.add(new ListItem(DetailItemType.TAB, model));
        delegate.onChangeSelectionStateForAllTabs();
        Assert.assertEquals(mModel.get(NUM_TABS_DESELECTED), tabItems.size());
    }

    @Test
    public void testRestoreTabsMediator_createReviewTabsScreenDelegateOnSelectedTabsChosen() {
        RestoreTabsMetricsHelper.setPromoShownCount(1);
        mModel.set(VISIBLE, true);
        mMediator.setCurrentScreen(REVIEW_TABS_SCREEN);
        RestoreTabsDetailScreenCoordinator.Delegate delegate =
                mModel.get(REVIEW_TABS_SCREEN_DELEGATE);

        ModelList tabItems = mModel.get(REVIEW_TABS_MODEL_LIST);
        ForeignSessionTab tab1 = new ForeignSessionTab(JUnitTestGURLs.URL_1, "title", 32L, 32L, 0);
        PropertyModel model1 = TabItemProperties.create(/* tab= */ tab1, /* isSelected= */ true);
        tabItems.add(new ListItem(DetailItemType.TAB, model1));
        ForeignSessionTab tab2 = new ForeignSessionTab(JUnitTestGURLs.URL_1, "title", 32L, 32L, 0);
        PropertyModel model2 = TabItemProperties.create(/* tab= */ tab2, /* isSelected= */ false);
        tabItems.add(new ListItem(DetailItemType.TAB, model2));

        // Only add the selected tab
        List<ForeignSessionTab> tabs = new ArrayList<>();
        tabs.add(tab1);

        ForeignSessionWindow window = new ForeignSessionWindow(31L, 1, tabs);
        List<ForeignSessionWindow> windows = new ArrayList<>();
        windows.add(window);

        ForeignSession session =
                new ForeignSession("tag", "John's iPhone 6", 32L, windows, FormFactor.PHONE);
        List<ForeignSession> sessions = new ArrayList<>();
        sessions.add(session);

        mMediator.showHomeScreen(mForeignSessionHelper, sessions, mDelegate);
        delegate.onSelectedTabsChosen();
        verify(mDelegate).getGTSTabListModelSize();
        verify(mForeignSessionHelper)
                .openForeignSessionTabsAsBackgroundTabs(
                        tabs, mModel.get(SELECTED_DEVICE), mTabCreatorManager);
        verify(mTracker).notifyEvent(eq(RESTORE_TABS_USED));
        Assert.assertEquals(mModel.get(VISIBLE), false);
        verify(mDelegate).scrollGTSToRestoredTabs(0);
        RestoreTabsMetricsHelper.setPromoShownCount(0);
    }

    @Test
    public void testRestoreTabsMediator_toggleTabSelectedStateAllTabsTrueWithDelegate() {
        mMediator.setCurrentScreen(REVIEW_TABS_SCREEN);
        ForeignSessionTab tab1 = new ForeignSessionTab(JUnitTestGURLs.URL_1, "title", 32L, 32L, 0);
        PropertyModel model1 = TabItemProperties.create(tab1, true);

        ForeignSessionTab tab2 = new ForeignSessionTab(JUnitTestGURLs.URL_1, "title", 32L, 32L, 0);
        PropertyModel model2 = TabItemProperties.create(tab2, true);

        ForeignSessionTab tab3 = new ForeignSessionTab(JUnitTestGURLs.URL_1, "title", 32L, 32L, 0);
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
        ForeignSessionTab tab1 = new ForeignSessionTab(JUnitTestGURLs.URL_1, "title", 32L, 32L, 0);
        PropertyModel model1 = TabItemProperties.create(tab1, false);

        ForeignSessionTab tab2 = new ForeignSessionTab(JUnitTestGURLs.URL_1, "title", 32L, 32L, 0);
        PropertyModel model2 = TabItemProperties.create(tab2, false);

        ForeignSessionTab tab3 = new ForeignSessionTab(JUnitTestGURLs.URL_1, "title", 32L, 32L, 0);
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
        ForeignSessionTab tab1 = new ForeignSessionTab(JUnitTestGURLs.URL_1, "title", 32L, 32L, 0);
        PropertyModel model1 = TabItemProperties.create(tab1, true);

        ForeignSessionTab tab2 = new ForeignSessionTab(JUnitTestGURLs.URL_1, "title", 32L, 32L, 0);
        PropertyModel model2 = TabItemProperties.create(tab2, false);

        ForeignSessionTab tab3 = new ForeignSessionTab(JUnitTestGURLs.URL_1, "title", 32L, 32L, 0);
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
        ForeignSessionTab tab1 = new ForeignSessionTab(JUnitTestGURLs.URL_1, "title", 32L, 32L, 0);
        PropertyModel model1 = TabItemProperties.create(tab1, true);

        ForeignSessionTab tab2 = new ForeignSessionTab(JUnitTestGURLs.URL_1, "title", 32L, 32L, 0);
        PropertyModel model2 = TabItemProperties.create(tab2, true);

        ForeignSessionTab tab3 = new ForeignSessionTab(JUnitTestGURLs.URL_1, "title", 32L, 32L, 0);
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
