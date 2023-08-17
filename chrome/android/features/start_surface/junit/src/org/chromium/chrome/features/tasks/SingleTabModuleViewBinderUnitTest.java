// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.features.tasks;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotEquals;
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
import static org.chromium.chrome.features.tasks.SingleTabViewProperties.TAB_THUMBNAIL;
import static org.chromium.chrome.features.tasks.SingleTabViewProperties.TITLE;
import static org.chromium.chrome.features.tasks.SingleTabViewProperties.URL;

import android.app.Activity;
import android.graphics.Bitmap;
import android.graphics.Matrix;
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

/** Tests for {@link SingleTabViewBinder} with {@link R.layout.single_tab_module_layout} */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class SingleTabModuleViewBinderUnitTest {
    private static final String TEST_TITLE = "test";
    private static final String TEST_URL = "www.foo.com";
    private final int mTabId = 1;
    private static final String HISTOGRAM_START_SURFACE_MODULE_CLICK = "StartSurface.Module.Click";

    private Activity mActivity;
    private SingleTabView mSingleTabModuleView;
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
        mActivity.setTheme(R.style.Theme_BrowserUI_DayNight);
        mSingleTabModuleView = (SingleTabView) mActivity.getLayoutInflater().inflate(
                R.layout.single_tab_module_layout, null);
        mActivity.setContentView(mSingleTabModuleView);

        mPropertyModel = new PropertyModel(SingleTabViewProperties.ALL_KEYS);
        mPropertyModelChangeProcessor = PropertyModelChangeProcessor.create(
                mPropertyModel, mSingleTabModuleView, SingleTabViewBinder::bind);
    }

    @After
    public void tearDown() throws Exception {
        mPropertyModelChangeProcessor.destroy();
        mPropertyModel = null;
        mSingleTabModuleView = null;
        mActivity = null;
    }

    private boolean isViewVisible(int viewId) {
        return mSingleTabModuleView.findViewById(viewId).getVisibility() == View.VISIBLE;
    }

    @Test
    @SmallTest
    public void testVisibility() {
        mPropertyModel.set(IS_VISIBLE, true);
        assertTrue(isViewVisible(org.chromium.chrome.R.id.single_tab_view));

        mPropertyModel.set(IS_VISIBLE, false);
        assertFalse(isViewVisible(org.chromium.chrome.R.id.single_tab_view));
    }

    @Test
    @SmallTest
    public void testSetTitle() {
        TextView title = mSingleTabModuleView.findViewById(org.chromium.chrome.R.id.tab_title_view);
        assertEquals("", title.getText());

        mPropertyModel.set(TITLE, TEST_TITLE);
        assertEquals(TEST_TITLE, title.getText());
    }

    @Test
    @SmallTest
    public void testSetUrl() {
        TextView url = mSingleTabModuleView.findViewById(org.chromium.chrome.R.id.tab_url_view);
        assertEquals("", url.getText());

        mPropertyModel.set(URL, TEST_URL);
        assertEquals(TEST_URL, url.getText());
    }

    @Test
    @SmallTest
    public void testSetFavicon() {
        ImageView favicon =
                mSingleTabModuleView.findViewById(org.chromium.chrome.R.id.tab_favicon_view);
        assertNull(favicon.getDrawable());

        mPropertyModel.set(FAVICON, new BitmapDrawable());
        assertNotNull(favicon.getDrawable());
    }

    @Test
    @SmallTest
    public void testSetTabThumbnail() {
        // Fake a layout so the UI has a size.
        mSingleTabModuleView.measure(0, 0);
        mSingleTabModuleView.layout(0, 0, 100, 100);

        ImageView thumbnail = mSingleTabModuleView.findViewById(R.id.tab_thumbnail);
        assertNull(thumbnail.getDrawable());

        Bitmap bitmap = Bitmap.createBitmap(300, 400, Bitmap.Config.ALPHA_8);
        mPropertyModel.set(TAB_THUMBNAIL, bitmap);
        assertNotNull(thumbnail.getDrawable());

        assertNotEquals(new Matrix(), thumbnail.getImageMatrix());
    }

    @Test
    @SmallTest
    public void testSetTabThumbnailUpdateMatrixOnResize() {
        ImageView thumbnail = mSingleTabModuleView.findViewById(R.id.tab_thumbnail);
        assertNull(thumbnail.getDrawable());

        Bitmap bitmap = Bitmap.createBitmap(300, 400, Bitmap.Config.ALPHA_8);
        mPropertyModel.set(TAB_THUMBNAIL, bitmap);
        assertNotNull(thumbnail.getDrawable());

        Matrix identityMatrix = new Matrix();
        assertEquals(identityMatrix, thumbnail.getImageMatrix());

        // Fake a layout so the UI has a size.
        mSingleTabModuleView.measure(0, 0);
        mSingleTabModuleView.layout(0, 0, 100, 100);

        assertNotEquals(identityMatrix, thumbnail.getImageMatrix());
    }

    @Test
    @SmallTest
    public void testClickListener() {
        mPropertyModel.set(CLICK_LISTENER, mClickListener);
        mSingleTabModuleView.performClick();
        verify(mClickListener).onClick(any());
    }

    @Test
    @SmallTest
    public void testRecordHistogramSingleTabCardClick_StartSurface() {
        doReturn(mTabId).when(mTabModelSelector).getCurrentTabId();
        doReturn(false).when(mTabModelSelector).isIncognitoSelected();
        SingleTabSwitcherMediator mediator =
                new SingleTabSwitcherMediator(ContextUtils.getApplicationContext(), mPropertyModel,
                        mTabModelSelector, mTabListFaviconProvider, null, false);
        mediator.setOnTabSelectingListener(mOnTabSelectingListener);
        mSingleTabModuleView.performClick();
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
                (MarginLayoutParams) mSingleTabModuleView.getLayoutParams();
        assertEquals(0, marginLayoutParams.getMarginStart());

        mPropertyModel.set(LATERAL_MARGIN, lateralMargin);
        assertEquals(100, marginLayoutParams.getMarginStart());
    }
}
