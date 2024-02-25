// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.recent_tabs.ui;

import static org.chromium.chrome.browser.recent_tabs.ui.ForeignSessionItemProperties.ALL_KEYS;
import static org.chromium.chrome.browser.recent_tabs.ui.ForeignSessionItemProperties.IS_SELECTED;
import static org.chromium.chrome.browser.recent_tabs.ui.ForeignSessionItemProperties.ON_CLICK_LISTENER;
import static org.chromium.chrome.browser.recent_tabs.ui.ForeignSessionItemProperties.SESSION_PROFILE;

import android.app.Activity;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.ImageView;
import android.widget.TextView;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.recent_tabs.ForeignSessionHelper.ForeignSession;
import org.chromium.chrome.browser.recent_tabs.ForeignSessionHelper.ForeignSessionTab;
import org.chromium.chrome.browser.recent_tabs.ForeignSessionHelper.ForeignSessionWindow;
import org.chromium.chrome.browser.recent_tabs.R;
import org.chromium.components.sync_device_info.FormFactor;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;
import org.chromium.url.JUnitTestGURLs;

import java.util.ArrayList;
import java.util.Calendar;
import java.util.Date;
import java.util.List;

/** Tests for ForeignSessionItemViewBinder. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class ForeignSessionItemViewBinderUnitTest {
    private static final long JAN_1_1970 = new Date(70, Calendar.JANUARY, 1).getTime();

    private Activity mActivity;
    private View mForeignSessionItemView;
    private View mForeignSessionItemView2;
    private PropertyModel mModel;
    private PropertyModel mModel2;
    private PropertyModelChangeProcessor mPropertyModelChangeProcessor;
    private PropertyModelChangeProcessor mPropertyModelChangeProcessor2;

    @Before
    public void setUp() throws Exception {
        mActivity = Robolectric.buildActivity(Activity.class).setup().get();
        mForeignSessionItemView =
                LayoutInflater.from(mActivity)
                        .inflate(R.layout.restore_tabs_foreign_session_item, /* root= */ null);

        mModel =
                new PropertyModel.Builder(ALL_KEYS)
                        .with(
                                SESSION_PROFILE,
                                new ForeignSession(
                                        "tag",
                                        "John's iPhone 6",
                                        JAN_1_1970,
                                        new ArrayList<>(),
                                        FormFactor.PHONE))
                        .with(IS_SELECTED, true)
                        .with(
                                ON_CLICK_LISTENER,
                                () -> {
                                    mModel.set(IS_SELECTED, !mModel.get(IS_SELECTED));
                                })
                        .build();

        mPropertyModelChangeProcessor =
                PropertyModelChangeProcessor.create(
                        mModel, mForeignSessionItemView, ForeignSessionItemViewBinder::bind);

        ForeignSessionTab tab =
                new ForeignSessionTab(JUnitTestGURLs.URL_1, "title", JAN_1_1970, JAN_1_1970, 0);
        List<ForeignSessionTab> tabs = new ArrayList<>();
        tabs.add(tab);

        ForeignSessionWindow window = new ForeignSessionWindow(JAN_1_1970, 1, tabs);
        List<ForeignSessionWindow> windows = new ArrayList<>();
        windows.add(window);

        mForeignSessionItemView2 =
                LayoutInflater.from(mActivity)
                        .inflate(R.layout.restore_tabs_foreign_session_item, /* root= */ null);
        mModel2 =
                new PropertyModel.Builder(ALL_KEYS)
                        .with(
                                SESSION_PROFILE,
                                new ForeignSession(
                                        "tag2",
                                        "John's iPad Air",
                                        JAN_1_1970,
                                        windows,
                                        FormFactor.TABLET))
                        .with(IS_SELECTED, true)
                        .with(
                                ON_CLICK_LISTENER,
                                () -> {
                                    mModel.set(IS_SELECTED, !mModel.get(IS_SELECTED));
                                })
                        .build();
        mPropertyModelChangeProcessor2 =
                PropertyModelChangeProcessor.create(
                        mModel2, mForeignSessionItemView2, ForeignSessionItemViewBinder::bind);
    }

    @Test
    public void testSessionProfile_phone() {
        mModel.get(SESSION_PROFILE);
        TextView sessionNameView =
                mForeignSessionItemView.findViewById(R.id.restore_tabs_detail_sheet_device_name);
        Assert.assertEquals("John's iPhone 6", sessionNameView.getText());

        ImageView deviceIconView =
                mForeignSessionItemView.findViewById(R.id.restore_tabs_device_sheet_device_icon);
        Assert.assertNotNull(deviceIconView.getDrawable());

        TextView sessionInfoView =
                mForeignSessionItemView.findViewById(R.id.restore_tabs_detail_sheet_session_info);
        Assert.assertEquals("0 tabs, last updated: Jan 1, 1970", sessionInfoView.getText());
    }

    @Test
    public void testOnClickListener() {
        mModel.get(ON_CLICK_LISTENER);
        View onClickListener =
                mForeignSessionItemView.findViewById(R.id.restore_tabs_detail_sheet_device_view);
        Assert.assertNotNull(onClickListener);
        onClickListener.performClick();
        Assert.assertFalse(mModel.get(IS_SELECTED));
    }

    @Test
    public void testSetIsSelected() {
        ImageView selectedIcon1 =
                mForeignSessionItemView.findViewById(
                        R.id.restore_tabs_detail_sheet_device_item_selected_icon);
        Assert.assertEquals(View.VISIBLE, selectedIcon1.getVisibility());

        mModel.set(IS_SELECTED, false);

        ImageView selectedIcon2 =
                mForeignSessionItemView.findViewById(
                        R.id.restore_tabs_detail_sheet_device_item_selected_icon);
        Assert.assertEquals(View.GONE, selectedIcon2.getVisibility());
    }

    @Test
    public void testSessionProfile_tablet() {
        mModel2.get(SESSION_PROFILE);
        TextView sessionNameView =
                mForeignSessionItemView2.findViewById(R.id.restore_tabs_detail_sheet_device_name);
        Assert.assertEquals("John's iPad Air", sessionNameView.getText());

        ImageView deviceIconView =
                mForeignSessionItemView2.findViewById(R.id.restore_tabs_device_sheet_device_icon);
        Assert.assertNotNull(deviceIconView.getDrawable());

        TextView sessionInfoView =
                mForeignSessionItemView2.findViewById(R.id.restore_tabs_detail_sheet_session_info);
        Assert.assertEquals("1 tab, last updated: Jan 1, 1970", sessionInfoView.getText());
    }
}
