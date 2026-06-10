// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.search_engines.settings.custom_search_engine;

import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;

import android.content.Context;
import android.view.ContextThemeWrapper;
import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;

import androidx.test.core.app.ApplicationProvider;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.search_engines.R;
import org.chromium.chrome.browser.search_engines.settings.common.SiteSearchProperties.ViewType;
import org.chromium.chrome.browser.search_engines.settings.common.SiteSearchViewBinder;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.SimpleRecyclerViewAdapter;

/** Unit tests for {@link CustomSearchEngineAdapter}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config
public class CustomSearchEngineAdapterUnitTest {
    private Context mContext;
    private ModelList mModelList;
    private CustomSearchEngineAdapter mAdapter;
    private ViewGroup mParent;

    @Before
    public void setUp() {
        mContext =
                new ContextThemeWrapper(
                        ApplicationProvider.getApplicationContext(),
                        R.style.Theme_BrowserUI_DayNight);
        mModelList = new ModelList();
        mAdapter = new CustomSearchEngineAdapter(mContext, mModelList);
        mParent = new FrameLayout(mContext);
    }

    @Test
    public void testOnCreateViewHolder_searchEngineType() {
        SimpleRecyclerViewAdapter.ViewHolder holder =
                mAdapter.onCreateViewHolder(mParent, ViewType.SEARCH_ENGINE);
        assertNotNull(holder);
        View view = holder.itemView;
        assertNotNull(view);
        Object tag = view.getTag();
        assertTrue(tag instanceof SiteSearchViewBinder.ViewHolder);
    }
}
