// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.features.tasks;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.verify;

import static org.chromium.chrome.features.tasks.SingleTabViewProperties.CLICK_LISTENER;
import static org.chromium.chrome.features.tasks.SingleTabViewProperties.FAVICON;
import static org.chromium.chrome.features.tasks.SingleTabViewProperties.IS_VISIBLE;
import static org.chromium.chrome.features.tasks.SingleTabViewProperties.LATERAL_MARGIN;
import static org.chromium.chrome.features.tasks.SingleTabViewProperties.TITLE;

import android.app.Activity;
import android.graphics.drawable.BitmapDrawable;
import android.view.View;
import android.view.ViewGroup.MarginLayoutParams;
import android.widget.ImageView;
import android.widget.TextView;

import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;

import org.chromium.base.ContextUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tasks.tab_management.TabListFaviconProvider;
import org.chromium.chrome.browser.tasks.tab_management.TabSwitcher;
import org.chromium.chrome.browser.util.BrowserUiUtils;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/** Tests for {@link SingleTabViewBinder}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class SingleTabViewBinderUnitTest {
    private static final String TEST_TITLE = "test";
    private final int mTabId = 1;
    private static final String HISTOGRAM_START_SURFACE_MODULE_CLICK = "StartSurface.Module.Click";

    private Activity mActivity;
    private SingleTabView mSingleTabView;
    private PropertyModelChangeProcessor<PropertyModel, SingleTabView, PropertyKey>
            mPropertyModelChangeProcessor;
    private PropertyModel mPropertyModel;

    @Mock
    private View.OnClickListener mClickListener;
    @Mock
    private TabModelSelector mTabModelSelector;
    @Mock
    private TabSwitcher.OnTabSelectingListener mOnTabSelectingListener;
    @Mock
    private TabListFaviconProvider mTabListFaviconProvider;

    @Before
    public void setUp() throws Exception {
        MockitoAnnotations.initMocks(this);

        mActivity = Robolectric.buildActivity(Activity.class).setup().get();
        mSingleTabView = (SingleTabView) mActivity.getLayoutInflater().inflate(
                R.layout.single_tab_view_layout, null);
        mActivity.setContentView(mSingleTabView);

        mPropertyModel = new PropertyModel(SingleTabViewProperties.ALL_KEYS);
        mPropertyModelChangeProcessor = PropertyModelChangeProcessor.create(
                mPropertyModel, mSingleTabView, SingleTabViewBinder::bind);
    }

    @After
    public void tearDown() throws Exception {
        mPropertyModelChangeProcessor.destroy();
        mPropertyModel = null;
        mSingleTabView = null;
        mActivity = null;
    }

    private boolean isViewVisible(int viewId) {
        return mSingleTabView.findViewById(viewId).getVisibility() == View.VISIBLE;
    }

    @Test
    @SmallTest
    public void testSetTitle() {
        mPropertyModel.set(IS_VISIBLE, true);
        assertTrue(isViewVisible(R.id.single_tab_view));
        TextView title = mSingleTabView.findViewById(R.id.tab_title_view);
        assertEquals("", title.getText());

        mPropertyModel.set(TITLE, TEST_TITLE);
        assertEquals(TEST_TITLE, title.getText());

        mPropertyModel.set(IS_VISIBLE, false);
        assertFalse(isViewVisible(R.id.single_tab_view));
    }

    @Test
    @SmallTest
    public void testSetFavicon() {
        mPropertyModel.set(IS_VISIBLE, true);
        assertTrue(isViewVisible(R.id.single_tab_view));
        ImageView favicon = mSingleTabView.findViewById(R.id.tab_favicon_view);
        assertNull(favicon.getDrawable());

        mPropertyModel.set(FAVICON, new BitmapDrawable());
        assertNotNull(favicon.getDrawable());

        mPropertyModel.set(IS_VISIBLE, false);
        assertFalse(isViewVisible(R.id.single_tab_view));
    }

    @Test
    @SmallTest
    public void testClickListener() {
        mPropertyModel.set(IS_VISIBLE, true);
        assertTrue(isViewVisible(R.id.single_tab_view));

        mPropertyModel.set(CLICK_LISTENER, mClickListener);
        mSingleTabView.performClick();
        verify(mClickListener).onClick(any());

        mPropertyModel.set(IS_VISIBLE, false);
        assertFalse(isViewVisible(R.id.single_tab_view));
    }

    @Test
    @SmallTest
    public void testRecordHistogramSingleTabCardClick_StartSurface() {
        doReturn(mTabId).when(mTabModelSelector).getCurrentTabId();
        doReturn(false).when(mTabModelSelector).isIncognitoSelected();
        SingleTabSwitcherMediator mediator =
                new SingleTabSwitcherMediator(ContextUtils.getApplicationContext(), mPropertyModel,
                        mTabModelSelector, mTabListFaviconProvider);
        mediator.setOnTabSelectingListener(mOnTabSelectingListener);
        mSingleTabView.performClick();
        assertEquals(HISTOGRAM_START_SURFACE_MODULE_CLICK
                        + " is not recorded correctly when clicking on the single tab card.",
                1,
                RecordHistogram.getHistogramValueCountForTesting(
                        HISTOGRAM_START_SURFACE_MODULE_CLICK,
                        BrowserUiUtils.ModuleTypeOnStartAndNTP.SINGLE_TAB_CARD));
    }

    @Test
    @SmallTest
    public void testStartMargin() {
        int lateralMargin = 100;
        MarginLayoutParams marginLayoutParams =
                (MarginLayoutParams) mSingleTabView.getLayoutParams();
        assertEquals(0, marginLayoutParams.getMarginStart());

        mPropertyModel.set(LATERAL_MARGIN, lateralMargin);
        assertEquals(100, marginLayoutParams.getMarginStart());
    }
}
