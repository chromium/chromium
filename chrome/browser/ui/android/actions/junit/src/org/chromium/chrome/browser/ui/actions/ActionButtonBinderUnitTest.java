// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.actions;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
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
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;

import org.chromium.base.Callback;
import org.chromium.base.MathUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.RobolectricUtil;
import org.chromium.chrome.browser.ui.actions.button.ButtonState;
import org.chromium.chrome.browser.user_education.IphCommand;
import org.chromium.chrome.browser.user_education.UserEducationHelper;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/** Unit tests for {@link ActionButtonBinder}. */
@RunWith(BaseRobolectricTestRunner.class)
public class ActionButtonBinderUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private Drawable mDrawable;
    @Mock private Callback<View> mOnPressCallback;
    @Mock private Callback<View> mOnLongPressCallback;
    @Mock private IphIntent mIphIntent;
    @Mock private UserEducationHelper mUserEducationHelper;

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

    @Test
    @SmallTest
    public void testButtonState() {
        mModel.set(ActionProperties.ON_PRESS_CALLBACK, mOnPressCallback);

        // Default state.
        mModel.set(ActionProperties.BUTTON_STATE, ButtonState.DEFAULT);
        assertEquals(1.0f, mView.getAlpha(), MathUtils.EPSILON);
        assertTrue(mView.isClickable());
        assertTrue(mView.isEnabled());

        // Change state to UNCLICKABLE.
        mModel.set(ActionProperties.BUTTON_STATE, ButtonState.UNCLICKABLE);
        assertEquals(1.0f, mView.getAlpha(), MathUtils.EPSILON);
        assertFalse(mView.isClickable());
        assertTrue(mView.isEnabled()); // Remains enabled to avoid grey-out blink

        // Change to INVISIBLE_AND_CLICKABLE.
        mModel.set(ActionProperties.BUTTON_STATE, ButtonState.INVISIBLE_AND_CLICKABLE);
        assertEquals(0.0f, mView.getAlpha(), MathUtils.EPSILON);
        assertEquals(View.VISIBLE, mView.getVisibility());
        assertTrue(mView.isClickable());
        assertTrue(mView.isEnabled()); // Remains enabled to avoid grey-out blink

        // Change back to DEFAULT.
        mModel.set(ActionProperties.BUTTON_STATE, ButtonState.DEFAULT);
        assertEquals(1.0f, mView.getAlpha(), MathUtils.EPSILON);
        assertTrue(mView.isClickable());
        assertTrue(mView.isEnabled());
    }

    @Test
    @SmallTest
    public void testIphIntent() {
        mModel.set(ActionProperties.USER_EDUCATION_HELPER, mUserEducationHelper);
        mModel.set(ActionProperties.IPH_INTENT, mIphIntent);
        RobolectricUtil.runAllBackgroundAndUi();

        verify(mIphIntent).tryShow(mView, mUserEducationHelper);
    }

    @Test(expected = AssertionError.class)
    @SmallTest
    public void testIphIntent_NullUserEducationHelper() {
        mModel.set(ActionProperties.USER_EDUCATION_HELPER, null);
        mModel.set(ActionProperties.IPH_INTENT, mIphIntent);
    }

    @Test
    @SmallTest
    public void testIphIntent_NullIphIntent() {
        mModel.set(ActionProperties.USER_EDUCATION_HELPER, mUserEducationHelper);
        mModel.set(ActionProperties.IPH_INTENT, null);

        verify(mIphIntent, never()).tryShow(mView, mUserEducationHelper);
    }

    @Test
    @SmallTest
    public void testIphIntent_MultipleViews() {
        ImageView view2 = new ImageView(mActivity);
        PropertyModelChangeProcessor.create(mModel, view2, ActionButtonBinder::bind);

        mModel.set(ActionProperties.USER_EDUCATION_HELPER, mUserEducationHelper);
        IphIntent realIphIntent =
                new IphIntent.Builder("TestFeature")
                        .setStringResId(android.R.string.ok)
                        .setAccessibilityResId(android.R.string.ok)
                        .build();
        assertFalse(realIphIntent.hasBeenShown());
        RobolectricUtil.runAllBackgroundAndUi();

        mModel.set(ActionProperties.IPH_INTENT, realIphIntent);
        assertTrue(realIphIntent.hasBeenShown());

        ArgumentCaptor<IphCommand> captor = ArgumentCaptor.forClass(IphCommand.class);
        verify(mUserEducationHelper, times(1)).requestShowIph(captor.capture());

        // Ensure that the IPH intent is only shown on the first view.
        assertEquals(mView, captor.getValue().anchorView);
    }
}
