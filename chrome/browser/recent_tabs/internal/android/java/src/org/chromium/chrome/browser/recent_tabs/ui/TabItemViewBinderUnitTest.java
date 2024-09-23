// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.recent_tabs.ui;

import static org.mockito.Mockito.when;

import static org.chromium.chrome.browser.recent_tabs.ui.TabItemProperties.ALL_KEYS;
import static org.chromium.chrome.browser.recent_tabs.ui.TabItemProperties.FOREIGN_SESSION_TAB;
import static org.chromium.chrome.browser.recent_tabs.ui.TabItemProperties.IS_SELECTED;
import static org.chromium.chrome.browser.recent_tabs.ui.TabItemProperties.ON_CLICK_LISTENER;

import android.app.Activity;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.CheckBox;
import android.widget.ImageView;
import android.widget.TextView;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.browser.recent_tabs.ForeignSessionHelper.ForeignSessionTab;
import org.chromium.chrome.browser.recent_tabs.R;
import org.chromium.chrome.browser.recent_tabs.ui.TabItemViewBinder.BindContext;
import org.chromium.chrome.browser.ui.favicon.FaviconHelper;
import org.chromium.chrome.browser.ui.favicon.FaviconHelper.DefaultFaviconHelper;
import org.chromium.chrome.browser.ui.favicon.FaviconHelperJni;
import org.chromium.chrome.browser.ui.favicon.FaviconUtils;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;
import org.chromium.url.JUnitTestGURLs;

/** Tests for TabItemViewBinder. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class TabItemViewBinderUnitTest {
    @Rule public JniMocker jniMocker = new JniMocker();

    @Mock FaviconHelper.Natives mFaviconHelperJniMock;
    @Mock Profile mProfile;

    private Activity mActivity;
    private View mTabItemView;
    private PropertyModel mModel;
    private PropertyModelChangeProcessor mPropertyModelChangeProcessor;
    private BindContext mBindContext;
    private FaviconHelper mFaviconHelper;

    @Before
    public void setUp() throws Exception {
        MockitoAnnotations.initMocks(this);
        ProfileManager.setLastUsedProfileForTesting(mProfile);
        jniMocker.mock(FaviconHelperJni.TEST_HOOKS, mFaviconHelperJniMock);
        when(mFaviconHelperJniMock.init()).thenReturn(1L);
        mActivity = Robolectric.buildActivity(Activity.class).setup().get();
        mFaviconHelper = new FaviconHelper();
        mBindContext =
                new BindContext(
                        new DefaultFaviconHelper(),
                        FaviconUtils.createCircularIconGenerator(mActivity),
                        mFaviconHelper,
                        mProfile);

        mTabItemView =
                LayoutInflater.from(mActivity)
                        .inflate(R.layout.restore_tabs_tab_item, /* root= */ null);

        mModel =
                new PropertyModel.Builder(ALL_KEYS)
                        .with(
                                FOREIGN_SESSION_TAB,
                                new ForeignSessionTab(JUnitTestGURLs.URL_1, "title", 32L, 32L, 0))
                        .with(IS_SELECTED, true)
                        .with(
                                ON_CLICK_LISTENER,
                                () -> {
                                    mModel.set(IS_SELECTED, !mModel.get(IS_SELECTED));
                                })
                        .build();

        mPropertyModelChangeProcessor =
                PropertyModelChangeProcessor.create(
                        mModel,
                        mTabItemView,
                        (tabModel, tabView, tabPropertyKey) -> {
                            TabItemViewBinder.bind(tabModel, tabView, tabPropertyKey, mBindContext);
                        });
    }

    @After
    public void tearDown() {
        mFaviconHelper.destroy();
    }

    @Test
    public void testForeignSessionTab() {
        mModel.get(FOREIGN_SESSION_TAB);
        ImageView faviconView =
                mTabItemView.findViewById(R.id.restore_tabs_review_tabs_screen_favicon);
        Assert.assertNotNull(faviconView.getDrawable());

        TextView tabNameView = mTabItemView.findViewById(R.id.restore_tabs_detail_sheet_tab_name);
        Assert.assertEquals("title", tabNameView.getText());

        TextView tabInfoView = mTabItemView.findViewById(R.id.restore_tabs_detail_sheet_tab_info);
        Assert.assertEquals("https://www.one.com/", tabInfoView.getText());

        CheckBox checkBoxView = mTabItemView.findViewById(R.id.restore_tabs_tab_item_checkbox);
        Assert.assertTrue(checkBoxView.isChecked());
    }

    @Test
    public void testOnClickListener() {
        CheckBox checkBoxView1 = mTabItemView.findViewById(R.id.restore_tabs_tab_item_checkbox);
        Assert.assertTrue(checkBoxView1.isChecked());

        mModel.get(ON_CLICK_LISTENER);
        View onClickListener =
                mTabItemView.findViewById(R.id.restore_tabs_detail_sheet_review_tabs_view);
        Assert.assertNotNull(onClickListener);
        onClickListener.performClick();

        CheckBox checkBoxView2 = mTabItemView.findViewById(R.id.restore_tabs_tab_item_checkbox);
        Assert.assertFalse(checkBoxView2.isChecked());
    }
}
