// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.recent_tabs.ui;

import static org.chromium.chrome.browser.recent_tabs.RestoreTabsProperties.ALL_KEYS;
import static org.chromium.chrome.browser.recent_tabs.RestoreTabsProperties.CURRENT_SCREEN;
import static org.chromium.chrome.browser.recent_tabs.RestoreTabsProperties.DETAIL_SCREEN_BACK_CLICK_HANDLER;
import static org.chromium.chrome.browser.recent_tabs.RestoreTabsProperties.DETAIL_SCREEN_MODEL_LIST;
import static org.chromium.chrome.browser.recent_tabs.RestoreTabsProperties.DETAIL_SCREEN_TITLE;
import static org.chromium.chrome.browser.recent_tabs.RestoreTabsProperties.DEVICE_MODEL_LIST;
import static org.chromium.chrome.browser.recent_tabs.RestoreTabsProperties.NUM_TABS_DESELECTED;
import static org.chromium.chrome.browser.recent_tabs.RestoreTabsProperties.REVIEW_TABS_MODEL_LIST;
import static org.chromium.chrome.browser.recent_tabs.RestoreTabsProperties.ScreenType.DEVICE_SCREEN;
import static org.chromium.chrome.browser.recent_tabs.RestoreTabsProperties.ScreenType.HOME_SCREEN;
import static org.chromium.chrome.browser.recent_tabs.RestoreTabsProperties.VISIBLE;

import android.app.Activity;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.TextView;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.recent_tabs.ForeignSessionHelper.ForeignSession;
import org.chromium.chrome.browser.recent_tabs.R;
import org.chromium.chrome.browser.recent_tabs.RestoreTabsProperties.DetailItemType;
import org.chromium.components.sync_device_info.FormFactor;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

import java.util.ArrayList;

/** Tests for RestoreTabsDetailScreenViewBinder. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class RestoreTabsDetailScreenViewBinderUnitTest {
    private Activity mActivity;
    private View mRestoreTabsDetailView;
    private PropertyModel mModel;
    private PropertyModelChangeProcessor mPropertyModelChangeProcessor;

    @Before
    public void setUp() throws Exception {
        mActivity = Robolectric.buildActivity(Activity.class).setup().get();
        mRestoreTabsDetailView = LayoutInflater.from(mActivity).inflate(
                R.layout.restore_tabs_bottom_sheet, /*root=*/null);

        mModel = new PropertyModel.Builder(ALL_KEYS)
                         .with(VISIBLE, false)
                         .with(DEVICE_MODEL_LIST, new ModelList())
                         .with(REVIEW_TABS_MODEL_LIST, new ModelList())
                         .with(NUM_TABS_DESELECTED, 0)
                         .build();

        mPropertyModelChangeProcessor = PropertyModelChangeProcessor.create(mModel,
                new RestoreTabsDetailScreenViewBinder.ViewHolder(mRestoreTabsDetailView),
                RestoreTabsDetailScreenViewBinder::bind);
    }

    @Test
    public void testSetDeviceScreen() {
        ForeignSession session = new ForeignSession(
                "tag", "John's iPhone 6", 32L, new ArrayList<>(), FormFactor.PHONE);
        ModelList deviceItems = mModel.get(DEVICE_MODEL_LIST);
        PropertyModel model = ForeignSessionItemProperties.create(
                /*session=*/session, /*isSelected=*/true, /*onClickListener=*/() -> {});
        deviceItems.add(new ListItem(DetailItemType.DEVICE, model));

        mModel.set(DETAIL_SCREEN_TITLE, R.string.restore_tabs_device_screen_sheet_title);
        mModel.set(DETAIL_SCREEN_BACK_CLICK_HANDLER,
                () -> { mModel.set(CURRENT_SCREEN, HOME_SCREEN); });
        mModel.set(CURRENT_SCREEN, DEVICE_SCREEN);

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
        ForeignSession session = new ForeignSession(
                "tag", "John's iPhone 6", 32L, new ArrayList<>(), FormFactor.PHONE);
        ModelList deviceItems = mModel.get(DEVICE_MODEL_LIST);
        PropertyModel model = ForeignSessionItemProperties.create(
                /*session=*/session, /*isSelected=*/true, /*onClickListener=*/() -> {});
        deviceItems.add(new ListItem(DetailItemType.DEVICE, model));

        mModel.set(DETAIL_SCREEN_TITLE, R.string.restore_tabs_device_screen_sheet_title);
        mModel.set(CURRENT_SCREEN, DEVICE_SCREEN);
        mModel.set(DETAIL_SCREEN_MODEL_LIST, deviceItems);
    }
}
