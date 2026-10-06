// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.junit.Assert.assertEquals;

import static org.chromium.chrome.browser.tasks.tab_management.TabGridDialogProperties.PAGE_KEY_LISTENER;

import android.app.Activity;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;

import org.chromium.base.Callback;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.ui.modelutil.PropertyModel;

/** Robolectric tests for {@link TabGridDialogViewBinder}. */
@RunWith(BaseRobolectricTestRunner.class)
public class TabGridDialogViewBinderUnitTest {

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    private TabGridDialogViewBinder.ViewHolder mViewHolder;
    private TabListRecyclerView mContentView;
    @Mock Callback<TabKeyEventData> mPageKeyEventDataCallback;

    @Before
    public void setUp() {
        Activity activity = Robolectric.buildActivity(Activity.class).setup().get();
        activity.setTheme(R.style.Theme_BrowserUI_DayNight);
        mContentView = new TabListRecyclerView(activity, null);
        mViewHolder =
                new TabGridDialogViewBinder.ViewHolder(
                        new TabGridDialogToolbarView(activity, null),
                        mContentView,
                        new TabGridDialogView(activity, null));
    }

    @Test
    public void testPageKeyListenerCallback() {
        PropertyModel propertyModel =
                new PropertyModel.Builder(TabGridDialogProperties.ALL_KEYS)
                        .with(PAGE_KEY_LISTENER, mPageKeyEventDataCallback)
                        .build();

        TabGridDialogViewBinder.bind(propertyModel, mViewHolder, PAGE_KEY_LISTENER);

        assertEquals(
                mPageKeyEventDataCallback, mContentView.getPageKeyListenerCallbackForTesting());
    }
}
