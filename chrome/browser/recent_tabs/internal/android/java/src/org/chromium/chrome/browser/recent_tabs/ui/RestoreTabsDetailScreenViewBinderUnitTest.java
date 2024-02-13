// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.recent_tabs.ui;

import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import static org.chromium.chrome.browser.recent_tabs.RestoreTabsProperties.ALL_KEYS;
import static org.chromium.chrome.browser.recent_tabs.RestoreTabsProperties.CURRENT_SCREEN;
import static org.chromium.chrome.browser.recent_tabs.RestoreTabsProperties.DETAIL_SCREEN_BACK_CLICK_HANDLER;
import static org.chromium.chrome.browser.recent_tabs.RestoreTabsProperties.DETAIL_SCREEN_MODEL_LIST;
import static org.chromium.chrome.browser.recent_tabs.RestoreTabsProperties.DETAIL_SCREEN_TITLE;
import static org.chromium.chrome.browser.recent_tabs.RestoreTabsProperties.DEVICE_MODEL_LIST;
import static org.chromium.chrome.browser.recent_tabs.RestoreTabsProperties.NUM_TABS_DESELECTED;
import static org.chromium.chrome.browser.recent_tabs.RestoreTabsProperties.REVIEW_TABS_MODEL_LIST;
import static org.chromium.chrome.browser.recent_tabs.RestoreTabsProperties.REVIEW_TABS_SCREEN_DELEGATE;
import static org.chromium.chrome.browser.recent_tabs.RestoreTabsProperties.ScreenType.DEVICE_SCREEN;
import static org.chromium.chrome.browser.recent_tabs.RestoreTabsProperties.ScreenType.HOME_SCREEN;
import static org.chromium.chrome.browser.recent_tabs.RestoreTabsProperties.ScreenType.REVIEW_TABS_SCREEN;
import static org.chromium.chrome.browser.recent_tabs.RestoreTabsProperties.VISIBLE;

import android.app.Activity;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.TextView;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.recent_tabs.ForeignSessionHelper.ForeignSession;
import org.chromium.chrome.browser.recent_tabs.ForeignSessionHelper.ForeignSessionTab;
import org.chromium.chrome.browser.recent_tabs.R;
import org.chromium.chrome.browser.recent_tabs.RestoreTabsProperties.DetailItemType;
import org.chromium.components.sync_device_info.FormFactor;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;
import org.chromium.url.JUnitTestGURLs;

import java.util.ArrayList;

/** Tests for RestoreTabsDetailScreenViewBinder. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class RestoreTabsDetailScreenViewBinderUnitTest {
    @Mock private RestoreTabsDetailScreenCoordinator.Delegate mMockDelegate;
    @Mock private TabItemViewBinder.BindContext mBindContext;

    private Activity mActivity;
    private View mRestoreTabsDetailView;
    private PropertyModel mModel;
    private PropertyModelChangeProcessor mPropertyModelChangeProcessor;

    @Before
    public void setUp() throws Exception {
        MockitoAnnotations.initMocks(this);
        mActivity = Robolectric.buildActivity(Activity.class).setup().get();
        mRestoreTabsDetailView =
                LayoutInflater.from(mActivity)
                        .inflate(R.layout.restore_tabs_bottom_sheet, /* root= */ null);

        mModel =
                new PropertyModel.Builder(ALL_KEYS)
                        .with(VISIBLE, false)
                        .with(DEVICE_MODEL_LIST, new ModelList())
                        .with(REVIEW_TABS_MODEL_LIST, new ModelList())
                        .with(NUM_TABS_DESELECTED, 0)
                        .build();

        mPropertyModelChangeProcessor =
                PropertyModelChangeProcessor.create(
                        mModel,
                        new RestoreTabsDetailScreenViewBinder.ViewHolder(
                                mRestoreTabsDetailView, mBindContext),
                        RestoreTabsDetailScreenViewBinder::bind);
    }

    @Test
    public void testSetDeviceScreen() {
        ForeignSession session =
                new ForeignSession(
                        "tag", "John's iPhone 6", 32L, new ArrayList<>(), FormFactor.PHONE);
        ModelList deviceItems = mModel.get(DEVICE_MODEL_LIST);
        PropertyModel model =
                ForeignSessionItemProperties.create(
                        /* session= */ session,
                        /* isSelected= */ true,
                        /* onClickListener= */ () -> {});
        deviceItems.add(new ListItem(DetailItemType.DEVICE, model));

        mModel.set(DETAIL_SCREEN_TITLE, R.string.restore_tabs_device_screen_sheet_title);
        mModel.set(
                DETAIL_SCREEN_BACK_CLICK_HANDLER,
                () -> {
                    mModel.set(CURRENT_SCREEN, HOME_SCREEN);
                });
        mModel.set(CURRENT_SCREEN, DEVICE_SCREEN);

        View allTabsSelectionStateButton =
                mRestoreTabsDetailView.findViewById(
                        R.id.restore_tabs_button_change_all_tabs_selection);
        Assert.assertEquals(View.GONE, allTabsSelectionStateButton.getVisibility());

        View openTabsButton =
                mRestoreTabsDetailView.findViewById(R.id.restore_tabs_button_open_selected_tabs);
        Assert.assertEquals(View.GONE, openTabsButton.getVisibility());

        View backButton =
                mRestoreTabsDetailView.findViewById(R.id.restore_tabs_toolbar_back_image_button);
        Assert.assertNotNull(backButton);
        backButton.performClick();
        Assert.assertEquals(HOME_SCREEN, mModel.get(CURRENT_SCREEN));

        TextView detailViewTitle =
                mRestoreTabsDetailView.findViewById(R.id.restore_tabs_toolbar_title_text_view);
        Assert.assertEquals("Open from", detailViewTitle.getText());

        Assert.assertNotNull(
                mRestoreTabsDetailView.findViewById(R.id.restore_tabs_detail_screen_recycler_view));
    }

    @Test
    public void testOnDeviceScreen_setDetailScreenModelList() {
        ForeignSession session =
                new ForeignSession(
                        "tag", "John's iPhone 6", 32L, new ArrayList<>(), FormFactor.PHONE);
        ModelList deviceItems = mModel.get(DEVICE_MODEL_LIST);
        PropertyModel model =
                ForeignSessionItemProperties.create(
                        /* session= */ session,
                        /* isSelected= */ true,
                        /* onClickListener= */ () -> {});
        deviceItems.add(new ListItem(DetailItemType.DEVICE, model));

        mModel.set(DETAIL_SCREEN_TITLE, R.string.restore_tabs_device_screen_sheet_title);
        mModel.set(CURRENT_SCREEN, DEVICE_SCREEN);
        mModel.set(DETAIL_SCREEN_MODEL_LIST, deviceItems);
    }

    @Test
    public void testSetReviewTabsScreen() {
        mModel.set(REVIEW_TABS_SCREEN_DELEGATE, mMockDelegate);
        ForeignSessionTab tab1 = new ForeignSessionTab(JUnitTestGURLs.URL_1, "title", 32L, 32L, 0);
        PropertyModel model1 = TabItemProperties.create(tab1, true);

        ModelList tabItems = mModel.get(REVIEW_TABS_MODEL_LIST);
        tabItems.add(new ListItem(DetailItemType.TAB, model1));

        mModel.set(DETAIL_SCREEN_TITLE, R.string.restore_tabs_review_tabs_screen_sheet_title);
        mModel.set(
                DETAIL_SCREEN_BACK_CLICK_HANDLER,
                () -> {
                    mModel.set(CURRENT_SCREEN, HOME_SCREEN);
                });
        mModel.set(CURRENT_SCREEN, REVIEW_TABS_SCREEN);

        TextView allTabsSelectionStateButtonText =
                mRestoreTabsDetailView.findViewById(
                        R.id.restore_tabs_button_change_all_tabs_selection);
        Assert.assertEquals("Deselect all", allTabsSelectionStateButtonText.getText());

        View allTabsSelectionStateButton =
                mRestoreTabsDetailView.findViewById(
                        R.id.restore_tabs_button_change_all_tabs_selection);
        Assert.assertEquals(View.VISIBLE, allTabsSelectionStateButton.getVisibility());
        Assert.assertNotNull(allTabsSelectionStateButton);
        allTabsSelectionStateButton.performClick();
        verify(mMockDelegate, times(1)).onChangeSelectionStateForAllTabs();

        TextView openTabsButtonText =
                mRestoreTabsDetailView.findViewById(R.id.restore_tabs_button_open_selected_tabs);
        Assert.assertEquals("Open 1 tab", openTabsButtonText.getText());

        View openTabsButton =
                mRestoreTabsDetailView.findViewById(R.id.restore_tabs_button_open_selected_tabs);
        Assert.assertEquals(View.VISIBLE, openTabsButton.getVisibility());
        Assert.assertNotNull(openTabsButton);
        openTabsButton.performClick();
        verify(mMockDelegate, times(1)).onSelectedTabsChosen();

        View backButton =
                mRestoreTabsDetailView.findViewById(R.id.restore_tabs_toolbar_back_image_button);
        Assert.assertNotNull(backButton);
        backButton.performClick();
        Assert.assertEquals(HOME_SCREEN, mModel.get(CURRENT_SCREEN));

        TextView detailViewTitle =
                mRestoreTabsDetailView.findViewById(R.id.restore_tabs_toolbar_title_text_view);
        Assert.assertEquals("Review tabs", detailViewTitle.getText());

        Assert.assertNotNull(
                mRestoreTabsDetailView.findViewById(R.id.restore_tabs_detail_screen_recycler_view));
    }

    @Test
    public void testOnReviewTabsScreen_toggleTabSelectionState() {
        mModel.set(REVIEW_TABS_SCREEN_DELEGATE, mMockDelegate);
        ForeignSessionTab tab1 = new ForeignSessionTab(JUnitTestGURLs.URL_1, "title", 32L, 32L, 0);
        PropertyModel model1 = TabItemProperties.create(tab1, true);
        ForeignSessionTab tab2 = new ForeignSessionTab(JUnitTestGURLs.URL_1, "title2", 32L, 32L, 0);
        PropertyModel model2 = TabItemProperties.create(tab2, true);

        ModelList tabItems = mModel.get(REVIEW_TABS_MODEL_LIST);
        tabItems.add(new ListItem(DetailItemType.TAB, model1));
        tabItems.add(new ListItem(DetailItemType.TAB, model2));

        mModel.set(DETAIL_SCREEN_TITLE, R.string.restore_tabs_review_tabs_screen_sheet_title);
        mModel.set(CURRENT_SCREEN, REVIEW_TABS_SCREEN);

        TextView allTabsSelectionStateButtonText1 =
                mRestoreTabsDetailView.findViewById(
                        R.id.restore_tabs_button_change_all_tabs_selection);
        Assert.assertEquals("Deselect all", allTabsSelectionStateButtonText1.getText());

        TextView openTabsButtonText1 =
                mRestoreTabsDetailView.findViewById(R.id.restore_tabs_button_open_selected_tabs);
        Assert.assertEquals("Open 2 tabs", openTabsButtonText1.getText());

        mModel.set(NUM_TABS_DESELECTED, 1);

        TextView allTabsSelectionStateButtonText2 =
                mRestoreTabsDetailView.findViewById(
                        R.id.restore_tabs_button_change_all_tabs_selection);
        Assert.assertEquals("Select all", allTabsSelectionStateButtonText2.getText());

        TextView openTabsButtonText2 =
                mRestoreTabsDetailView.findViewById(R.id.restore_tabs_button_open_selected_tabs);
        Assert.assertEquals("Open 1 tab", openTabsButtonText2.getText());

        mModel.set(NUM_TABS_DESELECTED, 2);

        TextView allTabsSelectionStateButtonText3 =
                mRestoreTabsDetailView.findViewById(
                        R.id.restore_tabs_button_change_all_tabs_selection);
        Assert.assertEquals("Select all", allTabsSelectionStateButtonText3.getText());

        TextView openTabsButtonText3 =
                mRestoreTabsDetailView.findViewById(R.id.restore_tabs_button_open_selected_tabs);
        Assert.assertEquals("Open 0 tabs", openTabsButtonText3.getText());
    }
}
