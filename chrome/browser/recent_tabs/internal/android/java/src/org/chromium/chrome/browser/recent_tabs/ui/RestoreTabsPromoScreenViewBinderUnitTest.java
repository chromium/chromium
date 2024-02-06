// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.recent_tabs.ui;

import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import static org.chromium.chrome.browser.recent_tabs.RestoreTabsProperties.ALL_KEYS;
import static org.chromium.chrome.browser.recent_tabs.RestoreTabsProperties.CURRENT_SCREEN;
import static org.chromium.chrome.browser.recent_tabs.RestoreTabsProperties.DEVICE_MODEL_LIST;
import static org.chromium.chrome.browser.recent_tabs.RestoreTabsProperties.HOME_SCREEN_DELEGATE;
import static org.chromium.chrome.browser.recent_tabs.RestoreTabsProperties.NUM_TABS_DESELECTED;
import static org.chromium.chrome.browser.recent_tabs.RestoreTabsProperties.REVIEW_TABS_MODEL_LIST;
import static org.chromium.chrome.browser.recent_tabs.RestoreTabsProperties.SELECTED_DEVICE;
import static org.chromium.chrome.browser.recent_tabs.RestoreTabsProperties.ScreenType.DEVICE_SCREEN;
import static org.chromium.chrome.browser.recent_tabs.RestoreTabsProperties.ScreenType.HOME_SCREEN;
import static org.chromium.chrome.browser.recent_tabs.RestoreTabsProperties.VISIBLE;

import android.app.Activity;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.ImageView;
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
import java.util.Calendar;
import java.util.Date;

/** Tests for RestoreTabsPromoScreenViewBinder. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class RestoreTabsPromoScreenViewBinderUnitTest {
    private static final long JAN_1_1970 = new Date(70, Calendar.JANUARY, 1).getTime();
    @Mock private RestoreTabsPromoScreenCoordinator.Delegate mMockDelegate;

    private Activity mActivity;
    private View mRestoreTabsPromoView;
    private PropertyModel mModel;
    private PropertyModelChangeProcessor mPropertyModelChangeProcessor;

    @Before
    public void setUp() throws Exception {
        MockitoAnnotations.initMocks(this);
        mActivity = Robolectric.buildActivity(Activity.class).setup().get();
        mRestoreTabsPromoView =
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
                        new RestoreTabsPromoScreenViewBinder.ViewHolder(mRestoreTabsPromoView),
                        RestoreTabsPromoScreenViewBinder::bind);
    }

    @Test
    public void testOnHomeScreen_setSelectedDevice() {
        mModel.set(CURRENT_SCREEN, HOME_SCREEN);
        mModel.set(
                SELECTED_DEVICE,
                new ForeignSession(
                        "tag", "John's iPhone 6", JAN_1_1970, new ArrayList<>(), FormFactor.PHONE));
        TextView deviceNameView =
                mRestoreTabsPromoView.findViewById(R.id.restore_tabs_promo_sheet_device_name);
        TextView deviceInfoView =
                mRestoreTabsPromoView.findViewById(R.id.restore_tabs_promo_sheet_session_info);
        Assert.assertEquals("John's iPhone 6", deviceNameView.getText());
        Assert.assertEquals("0 tabs, last updated: Jan 1, 1970", deviceInfoView.getText());
    }

    @Test
    public void testSetHomeScreen() {
        mModel.set(HOME_SCREEN_DELEGATE, mMockDelegate);
        mModel.set(
                SELECTED_DEVICE,
                new ForeignSession(
                        "tag", "John's iPhone 6", JAN_1_1970, new ArrayList<>(), FormFactor.PHONE));
        mModel.set(CURRENT_SCREEN, HOME_SCREEN);

        TextView sheetSubtitleView =
                mRestoreTabsPromoView.findViewById(R.id.restore_tabs_promo_sheet_subtitle);
        Assert.assertEquals(
                "Choose the device you’d like to get tabs from", sheetSubtitleView.getText());

        TextView deviceNameView =
                mRestoreTabsPromoView.findViewById(R.id.restore_tabs_promo_sheet_device_name);
        TextView deviceInfoView =
                mRestoreTabsPromoView.findViewById(R.id.restore_tabs_promo_sheet_session_info);
        Assert.assertEquals("John's iPhone 6", deviceNameView.getText());
        Assert.assertEquals("0 tabs, last updated: Jan 1, 1970", deviceInfoView.getText());

        View sessionInfoLayout =
                mRestoreTabsPromoView.findViewById(R.id.restore_tabs_selected_device_view);
        Assert.assertNotNull(sessionInfoLayout);
        sessionInfoLayout.performClick();
        verify(mMockDelegate, times(1)).onShowDeviceList();

        View openTabsButton =
                mRestoreTabsPromoView.findViewById(R.id.restore_tabs_button_open_tabs);
        Assert.assertNotNull(openTabsButton);
        openTabsButton.performClick();
        verify(mMockDelegate, times(1)).onAllTabsChosen();

        TextView openTabsButtonText =
                mRestoreTabsPromoView.findViewById(R.id.restore_tabs_button_open_tabs);
        Assert.assertEquals("Open 0 tabs", openTabsButtonText.getText());

        View reviewTabsButton =
                mRestoreTabsPromoView.findViewById(R.id.restore_tabs_button_review_tabs);
        Assert.assertNotNull(reviewTabsButton);
        reviewTabsButton.performClick();
        verify(mMockDelegate, times(1)).onReviewTabsChosen();
    }

    @Test
    public void testOnHomeScreen_oneTab() {
        ForeignSessionTab tab1 =
                new ForeignSessionTab(JUnitTestGURLs.URL_1, "title", JAN_1_1970, JAN_1_1970, 0);
        ModelList tabItems = mModel.get(REVIEW_TABS_MODEL_LIST);
        PropertyModel model1 = TabItemProperties.create(/* tab= */ tab1, /* isSelected= */ true);
        tabItems.add(new ListItem(DetailItemType.TAB, model1));

        mModel.set(CURRENT_SCREEN, HOME_SCREEN);

        TextView openTabsButtonText =
                mRestoreTabsPromoView.findViewById(R.id.restore_tabs_button_open_tabs);
        Assert.assertEquals("Open 1 tab", openTabsButtonText.getText());
    }

    @Test
    public void testOnHomeScreen_multipleTab() {
        ForeignSessionTab tab1 =
                new ForeignSessionTab(JUnitTestGURLs.URL_1, "title", JAN_1_1970, JAN_1_1970, 0);
        ModelList tabItems = mModel.get(REVIEW_TABS_MODEL_LIST);
        PropertyModel model1 = TabItemProperties.create(/* tab= */ tab1, /* isSelected= */ true);
        tabItems.add(new ListItem(DetailItemType.TAB, model1));
        ForeignSessionTab tab2 =
                new ForeignSessionTab(JUnitTestGURLs.URL_1, "title", JAN_1_1970, JAN_1_1970, 0);
        PropertyModel model2 = TabItemProperties.create(/* tab= */ tab2, /* isSelected= */ true);
        tabItems.add(new ListItem(DetailItemType.TAB, model2));

        mModel.set(CURRENT_SCREEN, HOME_SCREEN);

        TextView openTabsButtonText =
                mRestoreTabsPromoView.findViewById(R.id.restore_tabs_button_open_tabs);
        Assert.assertEquals("Open 2 tabs", openTabsButtonText.getText());
    }

    @Test
    public void testOnHomeScreen_selectAndDeselectTabs() {
        ForeignSessionTab tab1 =
                new ForeignSessionTab(JUnitTestGURLs.URL_1, "title", JAN_1_1970, JAN_1_1970, 0);
        ModelList tabItems = mModel.get(REVIEW_TABS_MODEL_LIST);
        PropertyModel model1 = TabItemProperties.create(/* tab= */ tab1, /* isSelected= */ true);
        tabItems.add(new ListItem(DetailItemType.TAB, model1));
        ForeignSessionTab tab2 =
                new ForeignSessionTab(JUnitTestGURLs.URL_1, "title", JAN_1_1970, JAN_1_1970, 0);
        PropertyModel model2 = TabItemProperties.create(/* tab= */ tab2, /* isSelected= */ true);
        tabItems.add(new ListItem(DetailItemType.TAB, model2));

        mModel.set(NUM_TABS_DESELECTED, 1);
        mModel.set(CURRENT_SCREEN, HOME_SCREEN);

        TextView openTabsButtonText =
                mRestoreTabsPromoView.findViewById(R.id.restore_tabs_button_open_tabs);
        Assert.assertEquals("Open 1 tab", openTabsButtonText.getText());
    }

    @Test
    public void testSetHomeScreen_oneSessionInModelList() {
        mModel.set(HOME_SCREEN_DELEGATE, mMockDelegate);
        ForeignSession session =
                new ForeignSession(
                        "tag", "John's iPhone 6", JAN_1_1970, new ArrayList<>(), FormFactor.PHONE);
        ModelList deviceItems = mModel.get(DEVICE_MODEL_LIST);
        PropertyModel model =
                ForeignSessionItemProperties.create(
                        /* session= */ session,
                        /* isSelected= */ true,
                        /* onClickListener= */ () -> {
                            mModel.set(CURRENT_SCREEN, DEVICE_SCREEN);
                        });
        deviceItems.add(new ListItem(DetailItemType.DEVICE, model));

        ImageView expandSelectorView1 =
                mRestoreTabsPromoView.findViewById(R.id.restore_tabs_expand_icon_device_selection);
        Assert.assertEquals(View.VISIBLE, expandSelectorView1.getVisibility());

        mModel.set(CURRENT_SCREEN, HOME_SCREEN);

        ImageView expandSelectorView2 =
                mRestoreTabsPromoView.findViewById(R.id.restore_tabs_expand_icon_device_selection);
        Assert.assertEquals(View.GONE, expandSelectorView2.getVisibility());

        TextView sheetSubtitleView =
                mRestoreTabsPromoView.findViewById(R.id.restore_tabs_promo_sheet_subtitle);
        Assert.assertEquals(
                "You can get tabs from the device listed below", sheetSubtitleView.getText());

        View sessionInfoLayout =
                mRestoreTabsPromoView.findViewById(R.id.restore_tabs_selected_device_view);
        Assert.assertNotNull(sessionInfoLayout);
        sessionInfoLayout.performClick();
        Assert.assertNotEquals(DEVICE_SCREEN, mModel.get(CURRENT_SCREEN));
    }

    @Test
    public void testSetHomeScreen_sessionIsTablet() {
        mModel.set(HOME_SCREEN_DELEGATE, mMockDelegate);
        ForeignSession session =
                new ForeignSession(
                        "tag", "John's iPad Air", JAN_1_1970, new ArrayList<>(), FormFactor.TABLET);
        ModelList deviceItems = mModel.get(DEVICE_MODEL_LIST);
        PropertyModel model =
                ForeignSessionItemProperties.create(
                        /* session= */ session,
                        /* isSelected= */ true,
                        /* onClickListener= */ () -> {
                            mModel.set(CURRENT_SCREEN, DEVICE_SCREEN);
                        });
        deviceItems.add(new ListItem(DetailItemType.DEVICE, model));
        mModel.set(CURRENT_SCREEN, HOME_SCREEN);

        View sessionInfoLayout =
                mRestoreTabsPromoView.findViewById(R.id.restore_tabs_selected_device_view);
        Assert.assertNotNull(sessionInfoLayout);
        sessionInfoLayout.performClick();
        Assert.assertNotEquals(DEVICE_SCREEN, mModel.get(CURRENT_SCREEN));

        mModel.set(SELECTED_DEVICE, session);
        ImageView deviceIconView =
                mRestoreTabsPromoView.findViewById(R.id.restore_tabs_promo_sheet_device_icon);
        Assert.assertNotNull(deviceIconView.getDrawable());
    }
}
