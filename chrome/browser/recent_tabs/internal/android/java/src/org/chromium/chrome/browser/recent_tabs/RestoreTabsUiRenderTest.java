// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.recent_tabs;

import static androidx.test.espresso.matcher.ViewMatchers.withId;

import static org.mockito.Mockito.when;

import static org.chromium.chrome.browser.recent_tabs.RestoreTabsProperties.CURRENT_SCREEN;
import static org.chromium.chrome.browser.recent_tabs.RestoreTabsProperties.DEVICE_MODEL_LIST;
import static org.chromium.chrome.browser.recent_tabs.RestoreTabsProperties.NUM_TABS_DESELECTED;
import static org.chromium.chrome.browser.recent_tabs.RestoreTabsProperties.REVIEW_TABS_MODEL_LIST;
import static org.chromium.chrome.browser.recent_tabs.RestoreTabsProperties.SELECTED_DEVICE;
import static org.chromium.chrome.browser.recent_tabs.RestoreTabsProperties.ScreenType.HOME_SCREEN;

import android.app.Activity;
import android.view.View;
import android.widget.FrameLayout;

import androidx.appcompat.content.res.AppCompatResources;
import androidx.recyclerview.widget.RecyclerView;
import androidx.test.filters.MediumTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.BaseActivityTestRule;
import org.chromium.base.test.params.ParameterAnnotations;
import org.chromium.base.test.params.ParameterSet;
import org.chromium.base.test.params.ParameterizedRunner;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.browser.recent_tabs.ForeignSessionHelper.ForeignSession;
import org.chromium.chrome.browser.recent_tabs.ForeignSessionHelper.ForeignSessionTab;
import org.chromium.chrome.browser.recent_tabs.RestoreTabsProperties.DetailItemType;
import org.chromium.chrome.browser.recent_tabs.ui.ForeignSessionItemProperties;
import org.chromium.chrome.browser.recent_tabs.ui.TabItemProperties;
import org.chromium.chrome.browser.tabmodel.TabCreatorManager;
import org.chromium.chrome.browser.ui.favicon.FaviconHelper;
import org.chromium.chrome.browser.ui.favicon.FaviconHelperJni;
import org.chromium.chrome.test.ChromeJUnit4RunnerDelegate;
import org.chromium.chrome.test.util.ChromeRenderTestRule;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.sync_device_info.FormFactor;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.test.util.BlankUiTestActivity;
import org.chromium.ui.test.util.NightModeTestUtils;
import org.chromium.ui.test.util.RenderTestRule;
import org.chromium.ui.test.util.ViewUtils;
import org.chromium.url.JUnitTestGURLs;

import java.io.IOException;
import java.util.ArrayList;
import java.util.List;

/** Render tests for Restore Tabs UI elements */
@RunWith(ParameterizedRunner.class)
@ParameterAnnotations.UseRunnerDelegate(ChromeJUnit4RunnerDelegate.class)
@DoNotBatch(reason = "RestoreTabs tests different startup behaviours and shouldn't be batched.")
public class RestoreTabsUiRenderTest {
    @ParameterAnnotations.ClassParameter
    public static List<ParameterSet> sClassParams =
            new NightModeTestUtils.NightModeParams().getParameters();

    @Rule
    public final ChromeRenderTestRule mRenderTestRule =
            ChromeRenderTestRule.Builder.withPublicCorpus()
                    .setBugComponent(RenderTestRule.Component.UI_BROWSER_MOBILE_RECENT_TABS)
                    .setRevision(4)
                    .build();

    @Rule
    public BaseActivityTestRule<BlankUiTestActivity> mActivityTestRule =
            new BaseActivityTestRule<>(BlankUiTestActivity.class);

    @Rule public JniMocker jniMocker = new JniMocker();

    @Mock ForeignSessionHelper.Natives mForeignSessionHelperJniMock;
    @Mock FaviconHelper.Natives mFaviconHelperJniMock;
    @Mock private Profile mProfile;
    @Mock private TabCreatorManager mTabCreatorManager;
    @Mock private BottomSheetController mBottomSheetController;

    private RestoreTabsCoordinator mCoordinator;
    private View mView;
    private PropertyModel mModel;
    private FrameLayout mRootView;

    public RestoreTabsUiRenderTest(boolean nightModeEnabled) {
        NightModeTestUtils.setUpNightModeForBlankUiTestActivity(nightModeEnabled);
        mRenderTestRule.setNightModeEnabled(nightModeEnabled);
    }

    @Before
    public void setUp() throws InterruptedException {
        MockitoAnnotations.initMocks(this);
        ProfileManager.setLastUsedProfileForTesting(mProfile);
        jniMocker.mock(ForeignSessionHelperJni.TEST_HOOKS, mForeignSessionHelperJniMock);
        jniMocker.mock(FaviconHelperJni.TEST_HOOKS, mFaviconHelperJniMock);
        mActivityTestRule.launchActivity(null);
        when(mFaviconHelperJniMock.init()).thenReturn(1L);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Activity activity = mActivityTestRule.getActivity();

                    mCoordinator =
                            new RestoreTabsCoordinator(
                                    mActivityTestRule.getActivity(),
                                    mProfile,
                                    mTabCreatorManager,
                                    mBottomSheetController);
                    mView = mCoordinator.getContentViewForTesting();
                    mView.setBackground(
                            AppCompatResources.getDrawable(activity, R.drawable.menu_bg_tinted));
                    mModel = mCoordinator.getPropertyModelForTesting();

                    mRootView = new FrameLayout(activity);
                    activity.setContentView(mRootView);
                    mRootView.addView(mView);
                });
    }

    @After
    public void tearDownTest() throws Exception {
        ThreadUtils.runOnUiThreadBlocking(
                () -> NightModeTestUtils.tearDownNightModeForBlankUiTestActivity());
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    public void testPromoScreenSheet_allOptionsEnabled() throws IOException, InterruptedException {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    // 0 devices in DEVICE_MODEL_LIST and 1 selected tab in REVIEW_TABS_MODEL_LIST.
                    // Restore tabs button enabled and chevron/onClickListener for device view.
                    ForeignSessionTab tab =
                            new ForeignSessionTab(JUnitTestGURLs.URL_1, "title", 32L, 32L, 0);
                    ForeignSession session =
                            new ForeignSession(
                                    "tag",
                                    "John's iPhone 6",
                                    32L,
                                    new ArrayList<>(),
                                    FormFactor.PHONE);

                    ModelList tabItems = mModel.get(RestoreTabsProperties.REVIEW_TABS_MODEL_LIST);
                    tabItems.clear();
                    PropertyModel model =
                            TabItemProperties.create(/* tab= */ tab, /* isSelected= */ true);
                    model.set(TabItemProperties.ON_CLICK_LISTENER, () -> {});
                    tabItems.add(new ListItem(DetailItemType.TAB, model));

                    mModel.set(SELECTED_DEVICE, session);
                    mModel.set(CURRENT_SCREEN, HOME_SCREEN);
                });

        ViewUtils.waitForView(mRootView, withId(R.id.restore_tabs_promo_screen_sheet));
        // TODO(crbug.com/40268908): With transitions causing unclear goldens, there is no
        // particular view
        // that can be waited on hence the need to use a sleep for rendering a cleaner image.
        Thread.sleep(2000);
        mRenderTestRule.render(mRootView, "restore_tabs_promo_screen_all_enabled");
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    public void testPromoScreenSheet_disabledDeviceViewAndRestoreButtonWithTabletIcon()
            throws IOException, InterruptedException {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    // 1 device in DEVICE_MODEL_LIST and 0 selected tabs in REVIEW_TABS_MODEL_LIST.
                    // Restore tabs button disabled, tablet icon and no chevron/onClickListener for
                    // device view.
                    ForeignSession session =
                            new ForeignSession(
                                    "tag",
                                    "John's iPhone 6",
                                    32L,
                                    new ArrayList<>(),
                                    FormFactor.TABLET);

                    ModelList sessionItems = mModel.get(DEVICE_MODEL_LIST);
                    sessionItems.clear();
                    PropertyModel model =
                            ForeignSessionItemProperties.create(
                                    /* session= */ session,
                                    /* isSelected= */ false,
                                    /* onClickListener= */ () -> {});
                    sessionItems.add(new ListItem(DetailItemType.DEVICE, model));

                    mModel.set(SELECTED_DEVICE, session);
                    mModel.set(CURRENT_SCREEN, HOME_SCREEN);
                });

        ViewUtils.waitForView(mRootView, withId(R.id.restore_tabs_promo_screen_sheet));
        // TODO(crbug.com/40268908): With transitions causing unclear goldens, there is no
        // particular view
        // that can be waited on hence the need to use a sleep for rendering a cleaner image.
        Thread.sleep(2000);
        mRenderTestRule.render(mRootView, "restore_tabs_promo_screen_disabled_elements");
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    public void testDeviceScreenSheet_twoItemDecorationAndSelection()
            throws IOException, InterruptedException {
        // For simplicity, this test sets all listed devices as selected to test UI elements
        // instead of calling core logic functions to select the most recently accessed device.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    ForeignSession session1 =
                            new ForeignSession(
                                    "tag1",
                                    "John's iPhone 6",
                                    32L,
                                    new ArrayList<>(),
                                    FormFactor.PHONE);
                    ForeignSession session2 =
                            new ForeignSession(
                                    "tag2",
                                    "John's iPhone 7",
                                    33L,
                                    new ArrayList<>(),
                                    FormFactor.PHONE);

                    List<ForeignSession> sessions = new ArrayList<>();
                    sessions.add(session1);
                    sessions.add(session2);

                    ModelList sessionItems = mModel.get(DEVICE_MODEL_LIST);
                    sessionItems.clear();
                    for (ForeignSession session : sessions) {
                        PropertyModel model =
                                ForeignSessionItemProperties.create(
                                        /* session= */ session,
                                        /* isSelected= */ true,
                                        /* onClickListener= */ () -> {});
                        sessionItems.add(new ListItem(DetailItemType.DEVICE, model));
                    }

                    mModel.set(CURRENT_SCREEN, HOME_SCREEN);
                    mView.findViewById(R.id.restore_tabs_selected_device_view).performClick();
                });

        ViewUtils.waitForView(mRootView, withId(R.id.restore_tabs_detail_screen_sheet));
        // TODO(crbug.com/40268908): With transitions causing unclear goldens, there is no
        // particular view
        // that can be waited on hence the need to use a sleep for rendering a cleaner image.
        Thread.sleep(2000);
        mRenderTestRule.render(mRootView, "restore_tabs_detail_screen_two_item_decoration");
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    public void testDeviceScreenSheet_threeItemDecorationAndTablet()
            throws IOException, InterruptedException {
        // For simplicity, this test sets all listed devices as deselected instead of calling
        // core logic functions to select the most recently accessed device.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    ForeignSession session1 =
                            new ForeignSession(
                                    "tag1",
                                    "John's iPhone 6",
                                    32L,
                                    new ArrayList<>(),
                                    FormFactor.PHONE);
                    ForeignSession session2 =
                            new ForeignSession(
                                    "tag2",
                                    "John's iPhone 7",
                                    33L,
                                    new ArrayList<>(),
                                    FormFactor.PHONE);
                    ForeignSession session3 =
                            new ForeignSession(
                                    "tag3",
                                    "John's iPad Air",
                                    34L,
                                    new ArrayList<>(),
                                    FormFactor.TABLET);

                    List<ForeignSession> sessions = new ArrayList<>();
                    sessions.add(session1);
                    sessions.add(session2);
                    sessions.add(session3);

                    ModelList sessionItems = mModel.get(DEVICE_MODEL_LIST);
                    sessionItems.clear();
                    for (ForeignSession session : sessions) {
                        PropertyModel model =
                                ForeignSessionItemProperties.create(
                                        /* session= */ session,
                                        /* isSelected= */ false,
                                        /* onClickListener= */ () -> {});
                        sessionItems.add(new ListItem(DetailItemType.DEVICE, model));
                    }

                    mModel.set(CURRENT_SCREEN, HOME_SCREEN);
                    mView.findViewById(R.id.restore_tabs_selected_device_view).performClick();
                });

        ViewUtils.waitForView(mRootView, withId(R.id.restore_tabs_detail_screen_sheet));
        // TODO(crbug.com/40268908): With transitions causing unclear goldens, there is no
        // particular view
        // that can be waited on hence the need to use a sleep for rendering a cleaner image.
        Thread.sleep(2000);
        mRenderTestRule.render(mRootView, "restore_tabs_detail_screen_three_item_decoration");
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    public void testReviewTabsScreenSheet_allTabsSelected()
            throws IOException, InterruptedException {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    ForeignSessionTab tab1 =
                            new ForeignSessionTab(JUnitTestGURLs.URL_1, "title", 32L, 32L, 0);
                    ForeignSessionTab tab2 =
                            new ForeignSessionTab(JUnitTestGURLs.URL_1, "title2", 33L, 33L, 0);
                    ForeignSessionTab tab3 =
                            new ForeignSessionTab(JUnitTestGURLs.URL_1, "title3", 34L, 34L, 0);

                    List<ForeignSessionTab> tabs = new ArrayList<>();
                    tabs.add(tab1);
                    tabs.add(tab2);
                    tabs.add(tab3);

                    ModelList tabItems = mModel.get(REVIEW_TABS_MODEL_LIST);
                    tabItems.clear();
                    for (ForeignSessionTab tab : tabs) {
                        PropertyModel model =
                                TabItemProperties.create(/* tab= */ tab, /* isSelected= */ true);
                        tabItems.add(new ListItem(DetailItemType.TAB, model));
                    }

                    mModel.set(CURRENT_SCREEN, HOME_SCREEN);
                    mView.findViewById(R.id.restore_tabs_button_review_tabs).performClick();
                });

        ViewUtils.waitForView(mRootView, withId(R.id.restore_tabs_detail_screen_sheet));
        // TODO(crbug.com/40268908): With transitions causing unclear goldens, there is no
        // particular view
        // that can be waited on hence the need to use a sleep for rendering a cleaner image.
        Thread.sleep(2000);
        mRenderTestRule.render(mRootView, "restore_tabs_detail_screen_review_tabs_all_selected");
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    public void testReviewTabsScreenSheet_noTabsSelectedSingleTab()
            throws IOException, InterruptedException {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    ForeignSessionTab tab1 =
                            new ForeignSessionTab(JUnitTestGURLs.URL_1, "title", 32L, 32L, 0);

                    List<ForeignSessionTab> tabs = new ArrayList<>();
                    tabs.add(tab1);

                    ModelList tabItems = mModel.get(REVIEW_TABS_MODEL_LIST);
                    tabItems.clear();
                    for (ForeignSessionTab tab : tabs) {
                        PropertyModel model =
                                TabItemProperties.create(/* tab= */ tab, /* isSelected= */ false);
                        tabItems.add(new ListItem(DetailItemType.TAB, model));
                    }

                    mModel.set(CURRENT_SCREEN, HOME_SCREEN);
                    mView.findViewById(R.id.restore_tabs_button_review_tabs).performClick();
                    mModel.set(NUM_TABS_DESELECTED, 1);
                });

        ViewUtils.waitForView(mRootView, withId(R.id.restore_tabs_detail_screen_sheet));
        // TODO(crbug.com/40268908): With transitions causing unclear goldens, there is no
        // particular view
        // that can be waited on hence the need to use a sleep for rendering a cleaner image.
        Thread.sleep(2000);
        mRenderTestRule.render(
                mRootView, "restore_tabs_detail_screen_review_tabs_none_selected_single_tab");
    }

    @Test
    @MediumTest
    @Feature("RenderTest")
    public void testReviewTabsScreenSheet_fillScreenWithTabsScrolledToBottom()
            throws IOException, InterruptedException {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    ForeignSessionTab tab1 =
                            new ForeignSessionTab(JUnitTestGURLs.URL_1, "title", 32L, 32L, 0);
                    ForeignSessionTab tab2 =
                            new ForeignSessionTab(JUnitTestGURLs.URL_1, "title2", 33L, 33L, 0);
                    ForeignSessionTab tab3 =
                            new ForeignSessionTab(JUnitTestGURLs.URL_1, "title3", 34L, 34L, 0);
                    ForeignSessionTab tab4 =
                            new ForeignSessionTab(JUnitTestGURLs.URL_1, "title4", 35L, 35L, 0);
                    ForeignSessionTab tab5 =
                            new ForeignSessionTab(JUnitTestGURLs.URL_1, "title5", 36L, 36L, 0);
                    ForeignSessionTab tab6 =
                            new ForeignSessionTab(JUnitTestGURLs.URL_1, "title6", 37L, 37L, 0);
                    ForeignSessionTab tab7 =
                            new ForeignSessionTab(JUnitTestGURLs.URL_1, "title7", 38L, 38L, 0);
                    ForeignSessionTab tab8 =
                            new ForeignSessionTab(JUnitTestGURLs.URL_1, "title8", 39L, 39L, 0);

                    List<ForeignSessionTab> tabs = new ArrayList<>();
                    tabs.add(tab1);
                    tabs.add(tab2);
                    tabs.add(tab3);
                    tabs.add(tab4);
                    tabs.add(tab5);
                    tabs.add(tab6);
                    tabs.add(tab7);
                    tabs.add(tab8);

                    ModelList tabItems = mModel.get(REVIEW_TABS_MODEL_LIST);
                    tabItems.clear();
                    for (ForeignSessionTab tab : tabs) {
                        PropertyModel model =
                                TabItemProperties.create(/* tab= */ tab, /* isSelected= */ true);
                        tabItems.add(new ListItem(DetailItemType.TAB, model));
                    }

                    mModel.set(CURRENT_SCREEN, HOME_SCREEN);
                    mView.findViewById(R.id.restore_tabs_button_review_tabs).performClick();
                    RecyclerView recyclerView =
                            mView.findViewById(R.id.restore_tabs_detail_screen_recycler_view);
                    recyclerView.scrollToPosition(tabs.size() - 1);
                });

        ViewUtils.waitForView(mRootView, withId(R.id.restore_tabs_detail_screen_sheet));
        // TODO(crbug.com/40268908): With transitions causing unclear goldens, there is no
        // particular view
        // that can be waited on hence the need to use a sleep for rendering a cleaner image.
        Thread.sleep(2000);
        mRenderTestRule.render(
                mRootView,
                "restore_tabs_detail_screen_review_tabs_filled_screen_scrolled_to_bottom");
    }
}
