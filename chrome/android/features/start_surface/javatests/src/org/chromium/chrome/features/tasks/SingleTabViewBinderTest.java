// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.features.tasks;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.anyObject;
import static org.mockito.Mockito.verify;

import static org.chromium.chrome.features.tasks.SingleTabViewProperties.CLICK_LISTENER;
import static org.chromium.chrome.features.tasks.SingleTabViewProperties.FAVICON;
import static org.chromium.chrome.features.tasks.SingleTabViewProperties.IS_VISIBLE;
import static org.chromium.chrome.features.tasks.SingleTabViewProperties.TITLE;

import android.graphics.drawable.BitmapDrawable;
import android.view.View;
import android.widget.ImageView;
import android.widget.TextView;

import androidx.test.filters.SmallTest;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import org.chromium.base.test.UiThreadTest;
import org.chromium.chrome.R;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;
import org.chromium.ui.test.util.BlankUiTestActivityTestCase;

/** Tests for {@link SingleTabViewBinder}. */
@RunWith(ChromeJUnit4ClassRunner.class)
public class SingleTabViewBinderTest extends BlankUiTestActivityTestCase {
    private SingleTabView mSingleTabView;
    private PropertyModelChangeProcessor mPropertyModelChangeProcessor;
    private PropertyModel mPropertyModel;
    private String mTitle = "test";

    @Mock
    private View.OnClickListener mClickListener;

    @Override
    public void setUpTest() throws Exception {
        super.setUpTest();
        MockitoAnnotations.initMocks(this);

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mSingleTabView = (SingleTabView) getActivity().getLayoutInflater().inflate(
                    R.layout.single_tab_view_layout, null);
            getActivity().setContentView(mSingleTabView);

            mPropertyModel = new PropertyModel(SingleTabViewProperties.ALL_KEYS);
            mPropertyModelChangeProcessor = PropertyModelChangeProcessor.create(
                    mPropertyModel, mSingleTabView, SingleTabViewBinder::bind);
        });
    }

    @Override
    public void tearDownTest() throws Exception {
        TestThreadUtils.runOnUiThreadBlocking(mPropertyModelChangeProcessor::destroy);
        mPropertyModel = null;
        mSingleTabView = null;
    }

    private boolean isViewVisible(int viewId) {
        return mSingleTabView.findViewById(viewId).getVisibility() == View.VISIBLE;
    }

    @Test
    @UiThreadTest
    @SmallTest
    public void testSetTitle() {
        mPropertyModel.set(IS_VISIBLE, true);
        assertTrue(isViewVisible(R.id.single_tab_view));
        TextView title = mSingleTabView.findViewById(R.id.tab_title_view);
        assertEquals("", title.getText());

        mPropertyModel.set(TITLE, mTitle);
        assertEquals(mTitle, title.getText());

        mPropertyModel.set(IS_VISIBLE, false);
        assertFalse(isViewVisible(R.id.single_tab_view));
    }

    @Test
    @UiThreadTest
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
    @UiThreadTest
    @SmallTest
    public void testClickListener() {
        mPropertyModel.set(IS_VISIBLE, true);
        assertTrue(isViewVisible(R.id.single_tab_view));

        mPropertyModel.set(CLICK_LISTENER, mClickListener);
        mSingleTabView.performClick();
        verify(mClickListener).onClick(anyObject());

        mPropertyModel.set(IS_VISIBLE, false);
        assertFalse(isViewVisible(R.id.single_tab_view));
    }
}
