// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.search_resumption;

import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import android.app.Activity;
import android.view.View;

import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;

import org.chromium.base.Callback;
import org.chromium.base.shared_preferences.SharedPreferencesManager;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.search_resumption.SearchResumptionTileBuilder.OnSuggestionClickCallback;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;
import org.chromium.url.GURL;
import org.chromium.url.JUnitTestGURLs;

/** Tests for {@link SearchResumptionModuleView}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class SearchResumptionModuleViewUnitTest {
    private Activity mActivity;
    private SearchResumptionModuleView mModuleView;
    private View mHeaderView;
    private SearchResumptionTileContainerView mTilesView;
    private PropertyModel mPropertyModel;
    private PropertyModelChangeProcessor mPropertyModelChangeProcessor;

    @Mock Callback<Boolean> mOnClickedCallback;

    @Before
    public void setUp() throws Exception {
        MockitoAnnotations.initMocks(this);
        mActivity = Robolectric.buildActivity(Activity.class).setup().get();
        mPropertyModel = new PropertyModel(SearchResumptionModuleProperties.ALL_KEYS);
    }

    @After
    public void tearDown() throws Exception {
        if (mPropertyModelChangeProcessor != null) {
            mPropertyModelChangeProcessor.destroy();
        }
        mPropertyModel = null;
        mModuleView = null;
        mActivity = null;
    }

    @Test
    @SmallTest
    public void testVisibilityAllowInitially() {
        inflateModuleView();

        Assert.assertTrue(mTilesView.isExpanded());
        Assert.assertFalse(
                ChromeSharedPreferences.getInstance()
                        .readBoolean(
                                ChromePreferenceKeys.SEARCH_RESUMPTION_MODULE_COLLAPSE_ON_NTP,
                                false));
    }

    @Test
    @SmallTest
    public void testVisibilityDisallowInitially() {
        SharedPreferencesManager sharedPreferencesManager = ChromeSharedPreferences.getInstance();
        sharedPreferencesManager.writeBoolean(
                ChromePreferenceKeys.SEARCH_RESUMPTION_MODULE_COLLAPSE_ON_NTP, true);

        inflateModuleView();
        Assert.assertFalse(mTilesView.isExpanded());
        Assert.assertTrue(
                sharedPreferencesManager.readBoolean(
                        ChromePreferenceKeys.SEARCH_RESUMPTION_MODULE_COLLAPSE_ON_NTP, false));

        sharedPreferencesManager.writeBoolean(
                ChromePreferenceKeys.SEARCH_RESUMPTION_MODULE_COLLAPSE_ON_NTP, false);
    }

    @Test
    @SmallTest
    public void testVisible() {
        inflateModuleView();
        Assert.assertTrue(isViewVisible(mModuleView));

        mPropertyModel.set(SearchResumptionModuleProperties.IS_VISIBLE, false);
        Assert.assertFalse(isViewVisible(mModuleView));

        mPropertyModel.set(SearchResumptionModuleProperties.IS_VISIBLE, true);
        Assert.assertTrue(isViewVisible(mModuleView));
    }

    @Test
    @SmallTest
    public void testExpandCollapseCallback() {
        inflateModuleView();
        Assert.assertTrue(mTilesView.isExpanded());

        mPropertyModel.set(
                SearchResumptionModuleProperties.EXPAND_COLLAPSE_CLICK_CALLBACK,
                mOnClickedCallback);
        mHeaderView.performClick();
        verify(mOnClickedCallback, times(1)).onResult(false);
        Assert.assertFalse(mTilesView.isExpanded());

        mHeaderView.performClick();
        verify(mOnClickedCallback, times(1)).onResult(true);
        Assert.assertTrue(mTilesView.isExpanded());
    }

    @Test
    @SmallTest
    public void testDestroy() {
        inflateModuleView();
        Assert.assertTrue(mTilesView.isExpanded());
        SearchResumptionTileView tileView = inflateTileView();
        mTilesView.addView(tileView);
        Assert.assertEquals(1, mTilesView.getChildCount());

        mModuleView.destroy();
        Assert.assertEquals(0, mTilesView.getChildCount());
    }

    @Test
    @SmallTest
    public void testTileView() {
        SearchResumptionTileView tileView = inflateTileView();
        String text = "foo";
        GURL gUrl = JUnitTestGURLs.EXAMPLE_URL;

        tileView.updateSuggestionData(gUrl, text);
        Assert.assertEquals(text, tileView.getTextForTesting());
        Assert.assertEquals(text, tileView.getContentDescription());

        OnSuggestionClickCallback callback = Mockito.mock(OnSuggestionClickCallback.class);
        tileView.addOnSuggestionClickCallback(callback);
        Assert.assertTrue(tileView.hasOnClickListeners());

        tileView.destroy();
        Assert.assertFalse(tileView.hasOnClickListeners());
    }

    private void inflateModuleView() {
        mModuleView =
                (SearchResumptionModuleView)
                        mActivity
                                .getLayoutInflater()
                                .inflate(R.layout.search_resumption_module_layout, null);
        mActivity.setContentView(mModuleView);
        mHeaderView = mModuleView.findViewById(R.id.search_resumption_module_header);
        mTilesView = mModuleView.findViewById(R.id.search_resumption_module_tiles_container);
        mPropertyModelChangeProcessor =
                PropertyModelChangeProcessor.create(
                        mPropertyModel, mModuleView, new SearchResumptionModuleViewBinder());
    }

    private SearchResumptionTileView inflateTileView() {
        return (SearchResumptionTileView)
                mActivity
                        .getLayoutInflater()
                        .inflate(R.layout.search_resumption_module_tile_layout, null);
    }

    private boolean isViewVisible(View view) {
        return view.getVisibility() == View.VISIBLE;
    }
}
