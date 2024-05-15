// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import android.app.Activity;
import android.graphics.Bitmap;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.Drawable;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewPropertyAnimator;
import android.widget.ImageView;
import android.widget.TextView;

import androidx.test.ext.junit.rules.ActivityScenarioRule;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.Spy;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.Callback;
import org.chromium.base.supplier.LazyOneshotSupplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.bookmarks.ImprovedBookmarkRowProperties.ImageVisibility;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.listmenu.ListMenuButtonDelegate;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/** Unit tests for {@link ImprovedBookmarkRow}. */
@Batch(Batch.UNIT_TESTS)
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class ImprovedBookmarkRowTest {
    private static final String TITLE = "Test title";
    private static final String DESCRIPTION = "Test description";

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule
    public ActivityScenarioRule<TestActivity> mActivityScenarioRule =
            new ActivityScenarioRule<>(TestActivity.class);

    @Mock View mView;
    @Mock ViewGroup mViewGroup;
    @Mock ListMenuButtonDelegate mListMenuButtonDelegate;
    @Mock Runnable mPopupListener;
    @Mock Runnable mOpenBookmarkCallback;
    @Mock LazyOneshotSupplier<Drawable> mMockDrawableSupplier;

    ImageView mStartImageView;
    @Spy ViewPropertyAnimator mStartImageViewAnimator;

    @Captor ArgumentCaptor<Callback<Drawable>> mDrawableCallbackCaptor;

    Activity mActivity;
    ImprovedBookmarkRow mImprovedBookmarkRow;
    PropertyModel mModel;
    BitmapDrawable mDrawable;
    LazyOneshotSupplier<Drawable> mDrawableSupplier;
    LazyOneshotSupplier<Drawable> mNullDrawableSupplier;

    @Before
    public void setUp() {
        mActivityScenarioRule.getScenario().onActivity((activity) -> mActivity = activity);
        mStartImageView =
                spy(
                        new ImageView(mActivity) {
                            @Override
                            public ViewPropertyAnimator animate() {
                                ViewPropertyAnimator animator = super.animate();
                                mStartImageViewAnimator = spy(animator);
                                return mStartImageViewAnimator;
                            }
                        });
        mDrawable =
                new BitmapDrawable(
                        mActivity.getResources(),
                        Bitmap.createBitmap(100, 100, Bitmap.Config.ARGB_8888));
        mDrawableSupplier = LazyOneshotSupplier.fromValue(mDrawable);
        mNullDrawableSupplier = LazyOneshotSupplier.fromValue(null);
        mImprovedBookmarkRow = ImprovedBookmarkRow.buildView(mActivity, /* isVisual= */ true);

        mModel =
                new PropertyModel.Builder(ImprovedBookmarkRowProperties.ALL_KEYS)
                        .with(ImprovedBookmarkRowProperties.TITLE, TITLE)
                        .with(ImprovedBookmarkRowProperties.DESCRIPTION, DESCRIPTION)
                        .with(ImprovedBookmarkRowProperties.DESCRIPTION_VISIBLE, true)
                        .with(
                                ImprovedBookmarkRowProperties.LIST_MENU_BUTTON_DELEGATE,
                                mListMenuButtonDelegate)
                        .with(ImprovedBookmarkRowProperties.POPUP_LISTENER, mPopupListener)
                        .with(
                                ImprovedBookmarkRowProperties.ROW_CLICK_LISTENER,
                                mOpenBookmarkCallback)
                        .with(ImprovedBookmarkRowProperties.EDITABLE, true)
                        .with(
                                ImprovedBookmarkRowProperties.END_IMAGE_VISIBILITY,
                                ImageVisibility.MENU)
                        .build();

        PropertyModelChangeProcessor.create(
                mModel, mImprovedBookmarkRow, ImprovedBookmarkRowViewBinder::bind);
    }

    private void toggleSelection() {
        mModel.set(ImprovedBookmarkRowProperties.SELECTED, true);
        mModel.set(ImprovedBookmarkRowProperties.SELECTION_ACTIVE, true);
        mModel.set(ImprovedBookmarkRowProperties.SELECTED, false);
        mModel.set(ImprovedBookmarkRowProperties.SELECTION_ACTIVE, false);
    }

    @Test
    public void testTitleAndDescription() {
        Assert.assertEquals(
                TITLE, ((TextView) mImprovedBookmarkRow.findViewById(R.id.title)).getText());
        Assert.assertEquals(
                DESCRIPTION,
                ((TextView) mImprovedBookmarkRow.findViewById(R.id.description)).getText());
    }

    @Test
    public void testDescriptionVisibility() {
        mModel.set(ImprovedBookmarkRowProperties.DESCRIPTION_VISIBLE, false);
        Assert.assertEquals(
                View.GONE, mImprovedBookmarkRow.findViewById(R.id.description).getVisibility());
    }

    @Test
    public void testNullAccessoryViewClearsExistingViews() {
        mModel.set(ImprovedBookmarkRowProperties.ACCESSORY_VIEW, mView);
        Assert.assertEquals(
                0,
                ((ViewGroup) mImprovedBookmarkRow.findViewById(R.id.custom_content_container))
                        .indexOfChild(mView));

        mModel.set(ImprovedBookmarkRowProperties.ACCESSORY_VIEW, null);
        Assert.assertEquals(
                -1,
                ((ViewGroup) mImprovedBookmarkRow.findViewById(R.id.custom_content_container))
                        .indexOfChild(mView));
    }

    @Test
    public void testSelectedShowsCheck() {
        mModel.set(ImprovedBookmarkRowProperties.SELECTION_ACTIVE, true);
        mModel.set(ImprovedBookmarkRowProperties.SELECTED, true);
        Assert.assertEquals(
                View.VISIBLE, mImprovedBookmarkRow.findViewById(R.id.check_image).getVisibility());
        Assert.assertEquals(
                View.GONE, mImprovedBookmarkRow.findViewById(R.id.more).getVisibility());
    }

    @Test
    public void testUnselectedShowsLastActive() {
        View check = mImprovedBookmarkRow.findViewById(R.id.check_image);
        View more = mImprovedBookmarkRow.findViewById(R.id.more);
        View image = mImprovedBookmarkRow.findViewById(R.id.end_image);

        // More button is set as visible, so it should be visible after the selection transition.
        mModel.set(ImprovedBookmarkRowProperties.END_IMAGE_VISIBILITY, ImageVisibility.MENU);
        toggleSelection();
        Assert.assertEquals(View.GONE, check.getVisibility());
        Assert.assertEquals(View.GONE, image.getVisibility());
        Assert.assertEquals(View.VISIBLE, more.getVisibility());

        mModel.set(ImprovedBookmarkRowProperties.END_IMAGE_VISIBILITY, ImageVisibility.DRAWABLE);
        toggleSelection();
        Assert.assertEquals(View.GONE, check.getVisibility());
        Assert.assertEquals(View.GONE, more.getVisibility());
        Assert.assertEquals(View.VISIBLE, image.getVisibility());

        mModel.set(ImprovedBookmarkRowProperties.END_IMAGE_VISIBILITY, ImageVisibility.NONE);
        toggleSelection();
        Assert.assertEquals(View.GONE, check.getVisibility());
        Assert.assertEquals(View.GONE, more.getVisibility());
        Assert.assertEquals(View.GONE, image.getVisibility());
    }

    @Test
    public void testSelectionActive() {
        mModel.set(ImprovedBookmarkRowProperties.SELECTION_ACTIVE, true);
        assertFalse(mImprovedBookmarkRow.findViewById(R.id.more).isClickable());
        assertFalse(mImprovedBookmarkRow.findViewById(R.id.more).isEnabled());
        Assert.assertEquals(
                View.IMPORTANT_FOR_ACCESSIBILITY_NO,
                mImprovedBookmarkRow.findViewById(R.id.more).getImportantForAccessibility());
    }

    @Test
    public void testSelectionInactive() {
        mModel.set(ImprovedBookmarkRowProperties.SELECTION_ACTIVE, false);
        assertTrue(mImprovedBookmarkRow.findViewById(R.id.more).isClickable());
        assertTrue(mImprovedBookmarkRow.findViewById(R.id.more).isEnabled());
        Assert.assertEquals(
                View.IMPORTANT_FOR_ACCESSIBILITY_YES,
                mImprovedBookmarkRow.findViewById(R.id.more).getImportantForAccessibility());
    }

    @Test
    public void testListMenuButtonDelegateDoesNotChangeVisibility() {
        int visibility = mImprovedBookmarkRow.findViewById(R.id.more).getVisibility();
        mModel.set(ImprovedBookmarkRowProperties.LIST_MENU_BUTTON_DELEGATE, null);
        // Setting the delegate shouldn't affect visibility.
        Assert.assertEquals(
                visibility, mImprovedBookmarkRow.findViewById(R.id.more).getVisibility());
    }

    @Test
    public void testNotEditableButMenuVisibility() {
        mModel.set(ImprovedBookmarkRowProperties.END_IMAGE_VISIBILITY, ImageVisibility.MENU);
        mModel.set(ImprovedBookmarkRowProperties.EDITABLE, false);
        Assert.assertEquals(
                View.GONE, mImprovedBookmarkRow.findViewById(R.id.more).getVisibility());
    }

    @Test
    public void testStartImageVisibility() {
        mModel.set(ImprovedBookmarkRowProperties.START_IMAGE_VISIBILITY, ImageVisibility.DRAWABLE);
        Assert.assertEquals(
                View.VISIBLE, mImprovedBookmarkRow.findViewById(R.id.start_image).getVisibility());
        Assert.assertEquals(
                View.GONE, mImprovedBookmarkRow.findViewById(R.id.folder_view).getVisibility());

        mModel.set(
                ImprovedBookmarkRowProperties.START_IMAGE_VISIBILITY,
                ImageVisibility.FOLDER_DRAWABLE);
        Assert.assertEquals(
                View.GONE, mImprovedBookmarkRow.findViewById(R.id.start_image).getVisibility());
        Assert.assertEquals(
                View.VISIBLE, mImprovedBookmarkRow.findViewById(R.id.folder_view).getVisibility());
    }

    @Test
    public void testEndImageVisibility() {
        mModel.set(ImprovedBookmarkRowProperties.END_IMAGE_VISIBILITY, ImageVisibility.DRAWABLE);
        Assert.assertEquals(
                View.GONE, mImprovedBookmarkRow.findViewById(R.id.more).getVisibility());
        Assert.assertEquals(
                View.VISIBLE, mImprovedBookmarkRow.findViewById(R.id.end_image).getVisibility());
    }

    @Test
    public void testAccessoryViewHasParent() {
        doReturn(mViewGroup).when(mView).getParent();
        doAnswer(
                        (invocation) -> {
                            doReturn(null).when(mView).getParent();
                            return null;
                        })
                .when(mViewGroup)
                .removeView(mView);

        mModel.set(ImprovedBookmarkRowProperties.ACCESSORY_VIEW, mView);
        verify(mViewGroup).removeView(mView);
    }

    @Test
    public void testSetStartImageDrawable() {
        mImprovedBookmarkRow.setStartImageViewForTesting(mStartImageView);

        mModel.set(ImprovedBookmarkRowProperties.END_IMAGE_VISIBILITY, ImageVisibility.DRAWABLE);
        mModel.set(ImprovedBookmarkRowProperties.START_ICON_DRAWABLE, mDrawableSupplier);

        ShadowLooper.runUiThreadTasks();

        verify(mStartImageView).setImageDrawable(null);
        verify(mStartImageView).setImageDrawable(mDrawable);
        verify(mStartImageView).setAlpha(0f);
        verify(mStartImageView).animate();
        verify(mStartImageViewAnimator).alpha(1f);
        verify(mStartImageViewAnimator).setDuration(ImprovedBookmarkRow.BASE_ANIMATION_DURATION_MS);
        verify(mStartImageViewAnimator).start();
    }

    @Test
    public void testSetStartImageDrawable_nullDrawableDoesNotAnimate() {
        mImprovedBookmarkRow.setStartImageViewForTesting(mStartImageView);

        mModel.set(ImprovedBookmarkRowProperties.END_IMAGE_VISIBILITY, ImageVisibility.DRAWABLE);
        mModel.set(ImprovedBookmarkRowProperties.START_ICON_DRAWABLE, mNullDrawableSupplier);

        ShadowLooper.runUiThreadTasks();

        verify(mStartImageView, times(2)).setImageDrawable(null);
        verify(mStartImageView, never()).setAlpha(0f);
        verify(mStartImageView, never()).animate();
        verify(mStartImageViewAnimator, never()).alpha(1f);
        verify(mStartImageViewAnimator, never())
                .setDuration(ImprovedBookmarkRow.BASE_ANIMATION_DURATION_MS);
        verify(mStartImageViewAnimator, never()).start();
    }

    @Test
    public void testCancelAnimation() {
        // This is tricky because the supplier introduces a post by default. But if we wait, we risk
        // letting the animation finish. So use a mock/captor to make it synchronous.
        mModel.set(ImprovedBookmarkRowProperties.START_ICON_DRAWABLE, mMockDrawableSupplier);
        verify(mMockDrawableSupplier).onAvailable(mDrawableCallbackCaptor.capture());
        verify(mMockDrawableSupplier).get();
        mDrawableCallbackCaptor.getValue().onResult(mDrawable);
        assertTrue(mImprovedBookmarkRow.hasTransientState());

        mImprovedBookmarkRow.cancelAnimation();
        assertFalse(mImprovedBookmarkRow.hasTransientState());
    }

    @Test
    public void testClick() {
        mImprovedBookmarkRow.performClick();
        verify(mOpenBookmarkCallback).run();
    }

    @Test
    public void testLocalAndRemoteBookmarks() {
        View localBookmarkImageView = mImprovedBookmarkRow.findViewById(R.id.local_bookmark_image);
        mModel.set(ImprovedBookmarkRowProperties.IS_LOCAL_BOOKMARK, true);
        assertEquals(View.VISIBLE, localBookmarkImageView.getVisibility());

        mModel.set(ImprovedBookmarkRowProperties.IS_LOCAL_BOOKMARK, false);
        assertEquals(View.GONE, localBookmarkImageView.getVisibility());
    }
}
