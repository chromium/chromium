// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.actions;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;

import android.app.Activity;
import android.graphics.drawable.Drawable;
import android.view.View;
import android.widget.ImageView;

import androidx.test.filters.SmallTest;

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
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/** Unit tests for {@link ActionButtonBinder}. */
@RunWith(BaseRobolectricTestRunner.class)
public class ActionButtonBinderUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private Drawable mDrawable;
    @Mock private Callback<View> mOnPressCallback;
    @Mock private Callback<View> mOnLongPressCallback;

    private Activity mActivity;
    private ImageView mView;
    private PropertyModel mModel;

    @Before
    public void setUp() {
        mActivity = Robolectric.buildActivity(Activity.class).setup().get();
        mView = new ImageView(mActivity);
        mActivity.setContentView(mView);
        mModel = new PropertyModel.Builder(ActionProperties.ALL_KEYS).build();
        PropertyModelChangeProcessor.create(mModel, mView, ActionButtonBinder::bind);
    }

    @Test
    @SmallTest
    public void testIcon() {
        mModel.set(ActionProperties.ICON, mDrawable);
        assertEquals(mView.getDrawable(), mDrawable);
    }

    @Test
    @SmallTest
    public void testIcon_NullDrawable() {
        mModel.set(ActionProperties.ICON, mDrawable);
        mModel.set(ActionProperties.ICON, null);
        assertNull(mView.getDrawable());
    }

    @Test
    @SmallTest
    public void testContentDescription() {
        String description = "Test description";
        mModel.set(ActionProperties.CONTENT_DESCRIPTION, description);
        assertTrue(mView.getContentDescription().equals(description));
    }

    @Test
    @SmallTest
    public void testContentDescription_NullString() {
        mModel.set(ActionProperties.CONTENT_DESCRIPTION, "Test");
        mModel.set(ActionProperties.CONTENT_DESCRIPTION, null);
        assertNull(mView.getContentDescription());
    }

    @Test
    @SmallTest
    public void testOnPressCallback() {
        mModel.set(ActionProperties.ON_PRESS_CALLBACK, mOnPressCallback);
        mView.performClick();
        verify(mOnPressCallback).onResult(mView);
    }

    @Test
    @SmallTest
    public void testOnPressCallback_NullCallback() {
        mModel.set(ActionProperties.ON_PRESS_CALLBACK, mOnPressCallback);
        mModel.set(ActionProperties.ON_PRESS_CALLBACK, null);
        assertFalse(mView.isEnabled());
        mView.performClick();
        verify(mOnPressCallback, never()).onResult(mView);
    }

    @Test
    @SmallTest
    public void testOnLongPressCallback() {
        mModel.set(ActionProperties.ON_LONG_PRESS_CALLBACK, mOnLongPressCallback);
        mView.performLongClick();
        verify(mOnLongPressCallback).onResult(mView);
    }

    @Test
    @SmallTest
    public void testOnLongPressCallback_NullCallback() {
        mModel.set(ActionProperties.ON_LONG_PRESS_CALLBACK, mOnLongPressCallback);
        mModel.set(ActionProperties.ON_LONG_PRESS_CALLBACK, null);
        mView.performLongClick();
        verify(mOnLongPressCallback, never()).onResult(mView);
    }

    @Test
    @SmallTest
    public void testButtonEnabledState_ClickListeners() {
        mModel.set(ActionProperties.ON_PRESS_CALLBACK, null);
        mModel.set(ActionProperties.ON_LONG_PRESS_CALLBACK, null);
        assertFalse(mView.isEnabled());

        mModel.set(ActionProperties.ON_PRESS_CALLBACK, mOnPressCallback);
        assertTrue(mView.isEnabled());

        mModel.set(ActionProperties.ON_LONG_PRESS_CALLBACK, mOnLongPressCallback);
        assertTrue(mView.isEnabled());

        mModel.set(ActionProperties.ON_PRESS_CALLBACK, null);
        assertFalse(mView.isEnabled());

        mModel.set(ActionProperties.ON_LONG_PRESS_CALLBACK, null);
        assertFalse(mView.isEnabled());
    }
}
