// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.hub;

import static org.junit.Assert.assertEquals;

import static org.chromium.chrome.browser.hub.HubSearchBoxBackgroundProperties.ALL_KEYS;
import static org.chromium.chrome.browser.hub.HubSearchBoxBackgroundProperties.COLOR_SCHEME;
import static org.chromium.chrome.browser.hub.HubSearchBoxBackgroundProperties.SHOW_BACKGROUND;

import android.app.Activity;
import android.graphics.drawable.ColorDrawable;
import android.view.LayoutInflater;
import android.view.View;

import androidx.core.content.ContextCompat;
import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.Robolectric;
import org.robolectric.android.controller.ActivityController;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.components.browser_ui.styles.SemanticColorUtils;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/** Unit tests for {@link HubSearchBoxBackgroundViewBinder}. */
@RunWith(BaseRobolectricTestRunner.class)
public class HubSearchBoxBackgroundViewBinderUnitTest {
    private ActivityController<TestActivity> mActivityController;
    private Activity mActivity;
    private View mSearchBoxBackgroundView;
    private PropertyModel mModel;

    @Before
    public void setUp() {
        mActivityController = Robolectric.buildActivity(TestActivity.class).setup();
        mActivity = mActivityController.get();
        View hubToolbarLayout =
                LayoutInflater.from(mActivity).inflate(R.layout.hub_toolbar_layout, null);

        mSearchBoxBackgroundView = hubToolbarLayout.findViewById(R.id.search_box_background);
        mModel = new PropertyModel.Builder(ALL_KEYS).build();
        PropertyModelChangeProcessor.create(
                mModel, mSearchBoxBackgroundView, HubSearchBoxBackgroundViewBinder::bind);
    }

    @After
    public void tearDown() {
        mActivityController.close();
    }

    @Test
    @SmallTest
    public void testSearchBoxBackground_toggleVisibility() {
        mModel.set(SHOW_BACKGROUND, true);
        assertEquals(View.VISIBLE, mSearchBoxBackgroundView.getVisibility());

        mModel.set(SHOW_BACKGROUND, false);
        assertEquals(View.GONE, mSearchBoxBackgroundView.getVisibility());
    }

    @Test
    @SmallTest
    public void testSearchBoxBackground_checkColorScheme() {
        mModel.set(COLOR_SCHEME, HubColorScheme.DEFAULT);
        assertEquals(
                SemanticColorUtils.getDefaultBgColor(mActivity),
                ((ColorDrawable) mSearchBoxBackgroundView.getBackground()).getColor());

        mModel.set(COLOR_SCHEME, HubColorScheme.INCOGNITO);
        assertEquals(
                ContextCompat.getColor(mActivity, R.color.default_bg_color_dark),
                ((ColorDrawable) mSearchBoxBackgroundView.getBackground()).getColor());
    }
}
