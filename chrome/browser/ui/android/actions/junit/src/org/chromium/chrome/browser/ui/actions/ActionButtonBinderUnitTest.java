// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.actions;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import android.app.Activity;
import android.content.Context;
import android.content.res.Resources;
import android.graphics.Color;
import android.graphics.drawable.ColorDrawable;
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

    @Mock private Callback<View> mOnPressCallback;
    @Mock private Callback<View> mOnLongPressCallback;
    @Mock private IphIntent mIphIntent;
    @Mock private UserEducationHelper mUserEducationHelper;

    private Activity mActivity;
    private ImageView mView;
    private PropertyModel mModel;
    private Drawable mDrawable;

    private static class TestDelegatingView extends ImageView implements DelegatingActionView {
        private final View mTarget;

        public TestDelegatingView(Context context, View target) {
            super(context);
            mTarget = target;
        }

        @Override
        public View getTargetView() {
            return mTarget;
        }
    }

    @Before
    public void setUp() {
        mActivity = Robolectric.buildActivity(Activity.class).setup().get();
        mView = new ImageView(mActivity);
        mActivity.setContentView(mView);
        mModel = new PropertyModel.Builder(ActionProperties.ALL_KEYS).build();
        PropertyModelChangeProcessor.create(mModel, mView, ActionButtonBinder::bind);
        mDrawable = new ColorDrawable(Color.RED);
    }

    @Test
    @SmallTest
    public void testIconId() {
        int resId = android.R.drawable.ic_delete;
        mModel.set(ActionProperties.ICON_ID, resId);
        assertNotNull(mView.getDrawable());
    }

    @Test
    @SmallTest
    public void testIconId_NullResId() {
        mModel.set(ActionProperties.ICON_ID, android.R.drawable.ic_delete);
        mModel.set(ActionProperties.ICON_ID, Resources.ID_NULL);
        assertNull(mView.getDrawable());
    }

    @Test
    @SmallTest
    public void testIconDrawable() {
        mModel.set(ActionProperties.ICON_DRAWABLE, mDrawable);
        assertEquals(mView.getDrawable(), mDrawable);
    }

    @Test
    @SmallTest
    public void testIconDrawable_NullDrawable() {
        mModel.set(ActionProperties.ICON_DRAWABLE, mDrawable);
        mModel.set(ActionProperties.ICON_DRAWABLE, null);
        assertNull(mView.getDrawable());
    }

    @Test
    @SmallTest
    public void testIconDrawable_OverridesIconId() {
        int resId = android.R.drawable.ic_delete;
        Drawable resDrawable = mActivity.getDrawable(resId);

        assertNotEquals(resDrawable, mDrawable);

        mModel.set(ActionProperties.ICON_ID, resId);
        mModel.set(ActionProperties.ICON_DRAWABLE, mDrawable);

        assertEquals(mDrawable, mView.getDrawable());
        assertNotEquals(resDrawable, mView.getDrawable());
    }

    @Test
    @SmallTest
    public void testIconDrawable_FallbackToIconIdWhenNull() {
        int resId = android.R.drawable.ic_delete;

        mModel.set(ActionProperties.ICON_ID, resId);
        mModel.set(ActionProperties.ICON_DRAWABLE, mDrawable);
        mModel.set(ActionProperties.ICON_DRAWABLE, null);

        assertNotNull(mView.getDrawable());
    }

    @Test
    @SmallTest
    public void testContentDescription() {
        String description = "Test description";
        mModel.set(ActionProperties.CONTENT_DESCRIPTION_RESOLVER, context -> description);
        assertEquals(description, mView.getContentDescription());
    }

    @Test
    @SmallTest
    public void testContentDescription_PluralString() {
        int count = 5;
        int pluralResId = 12345;
        String expectedDescription = "5 items";

        PropertyModel model = new PropertyModel.Builder(ActionProperties.ALL_KEYS).build();
        ImageView mockView = spy(new ImageView(mActivity));
        Context mockContext = mock(Context.class);
        Resources mockResources = mock(Resources.class);

        doReturn(mockContext).when(mockView).getContext();
        doReturn(mockResources).when(mockContext).getResources();
        doReturn(expectedDescription)
                .when(mockResources)
                .getQuantityString(pluralResId, count, count);

        PropertyModelChangeProcessor.create(model, mockView, ActionButtonBinder::bind);

        model.set(
                ActionProperties.CONTENT_DESCRIPTION_RESOLVER,
                new ResourceTextResolver(pluralResId, count));

        assertEquals(expectedDescription, mockView.getContentDescription());
    }

    @Test
    @SmallTest
    public void testContentDescription_NullString() {
        mModel.set(ActionProperties.CONTENT_DESCRIPTION_RESOLVER, context -> "Test description");
        mModel.set(ActionProperties.CONTENT_DESCRIPTION_RESOLVER, null);
        assertEquals("", mView.getContentDescription());
    }

    @Test
    @SmallTest
    public void testTooltipText() {
        String tooltip = "Test tooltip";
        mModel.set(ActionProperties.TOOLTIP_TEXT_RESOLVER, context -> tooltip);
        assertEquals(tooltip, mView.getTooltipText());
    }

    @Test
    @SmallTest
    public void testTooltipText_NullString() {
        mModel.set(ActionProperties.TOOLTIP_TEXT_RESOLVER, context -> "Test tooltip");
        mModel.set(ActionProperties.TOOLTIP_TEXT_RESOLVER, null);
        assertNull(mView.getTooltipText());
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

    @Test
    @SmallTest
    public void testDelegation() {
        ImageView targetView = new ImageView(mActivity);
        TestDelegatingView delegatingView = new TestDelegatingView(mActivity, targetView);

        PropertyModel model = new PropertyModel.Builder(ActionProperties.ALL_KEYS).build();
        PropertyModelChangeProcessor.create(model, delegatingView, ActionButtonBinder::bind);

        assertNull(targetView.getDrawable());
        assertNull(delegatingView.getDrawable());

        int resId = android.R.drawable.ic_delete;
        model.set(ActionProperties.ICON_ID, resId);

        assertNotNull(targetView.getDrawable());
        assertNull(delegatingView.getDrawable());
    }
}
