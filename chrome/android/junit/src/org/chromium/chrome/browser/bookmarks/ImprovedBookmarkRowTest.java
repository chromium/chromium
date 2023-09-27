// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;

import android.app.Activity;
import android.graphics.Bitmap;
import android.graphics.drawable.BitmapDrawable;
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
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.bookmarks.ImprovedBookmarkRowProperties.ImageVisibility;
import org.chromium.components.browser_ui.widget.listmenu.ListMenuButtonDelegate;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/** Unit tests for {@link BookmarkToolbarMediator}. */
@Batch(Batch.UNIT_TESTS)
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class ImprovedBookmarkRowTest {
    private static final String TITLE = "Test title";
    private static final String DESCRIPTION = "Test description";

    @Rule
    public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule
    public ActivityScenarioRule<TestActivity> mActivityScenarioRule =
            new ActivityScenarioRule<>(TestActivity.class);

    @Mock
    View mView;
    @Mock
    ViewGroup mViewGroup;
    @Mock
    ListMenuButtonDelegate mListMenuButtonDelegate;
    @Mock
    Runnable mPopupListener;
    @Mock
    Runnable mOpenBookmarkCallback;
    @Mock
    ImageView mStartImageView;
    @Mock
    ViewPropertyAnimator mStartImageViewAnimator;

    Activity mActivity;
    ImprovedBookmarkRow mImprovedBookmarkRow;
    PropertyModel mModel;
    BitmapDrawable mDrawable;

    @Before
    public void setUp() {
        mActivityScenarioRule.getScenario().onActivity((activity) -> mActivity = activity);

        doReturn(mStartImageViewAnimator).when(mStartImageView).animate();

        mDrawable = new BitmapDrawable(
                mActivity.getResources(), Bitmap.createBitmap(100, 100, Bitmap.Config.ARGB_8888));
        mImprovedBookmarkRow = ImprovedBookmarkRow.buildView(mActivity, /*isVisual=*/true);

        mModel = new PropertyModel.Builder(ImprovedBookmarkRowProperties.ALL_KEYS)
                         .with(ImprovedBookmarkRowProperties.TITLE, TITLE)
                         .with(ImprovedBookmarkRowProperties.DESCRIPTION, DESCRIPTION)
                         .with(ImprovedBookmarkRowProperties.DESCRIPTION_VISIBLE, true)
                         .with(ImprovedBookmarkRowProperties.LIST_MENU_BUTTON_DELEGATE,
                                 mListMenuButtonDelegate)
                         .with(ImprovedBookmarkRowProperties.POPUP_LISTENER, mPopupListener)
                         .with(ImprovedBookmarkRowProperties.ROW_CLICK_LISTENER,
                                 (v) -> { mOpenBookmarkCallback.run(); })
                         .with(ImprovedBookmarkRowProperties.EDITABLE, true)
                         .with(ImprovedBookmarkRowProperties.END_IMAGE_VISIBILITY,
                                 ImageVisibility.MENU)
                         .build();

        PropertyModelChangeProcessor.create(
                mModel, mImprovedBookmarkRow, ImprovedBookmarkRowViewBinder::bind);
    }

    @Test
    public void testTitleAndDescription() {
        Assert.assertEquals(
                TITLE, ((TextView) mImprovedBookmarkRow.findViewById(R.id.title)).getText());
        Assert.assertEquals(DESCRIPTION,
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
        Assert.assertEquals(0,
                ((ViewGroup) mImprovedBookmarkRow.findViewById(R.id.custom_content_container))
                        .indexOfChild(mView));

        mModel.set(ImprovedBookmarkRowProperties.ACCESSORY_VIEW, null);
        Assert.assertEquals(-1,
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
    public void testUnselectedShowsMore() {
        mModel.set(ImprovedBookmarkRowProperties.SELECTION_ACTIVE, true);
        mModel.set(ImprovedBookmarkRowProperties.SELECTED, false);
        Assert.assertEquals(
                View.GONE, mImprovedBookmarkRow.findViewById(R.id.check_image).getVisibility());
        Assert.assertEquals(
                View.VISIBLE, mImprovedBookmarkRow.findViewById(R.id.more).getVisibility());
    }

    @Test
    public void testSelectionActive() {
        mModel.set(ImprovedBookmarkRowProperties.SELECTION_ACTIVE, true);
        Assert.assertFalse(mImprovedBookmarkRow.findViewById(R.id.more).isClickable());
        Assert.assertFalse(mImprovedBookmarkRow.findViewById(R.id.more).isEnabled());
        Assert.assertEquals(View.IMPORTANT_FOR_ACCESSIBILITY_NO,
                mImprovedBookmarkRow.findViewById(R.id.more).getImportantForAccessibility());
    }

    @Test
    public void testSelectionInactive() {
        mModel.set(ImprovedBookmarkRowProperties.SELECTION_ACTIVE, false);
        Assert.assertTrue(mImprovedBookmarkRow.findViewById(R.id.more).isClickable());
        Assert.assertTrue(mImprovedBookmarkRow.findViewById(R.id.more).isEnabled());
        Assert.assertEquals(View.IMPORTANT_FOR_ACCESSIBILITY_YES,
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
        Assert.assertEquals(View.VISIBLE,
                mImprovedBookmarkRow.findViewById(R.id.start_image_container).getVisibility());
        Assert.assertEquals(
                View.GONE, mImprovedBookmarkRow.findViewById(R.id.folder_view).getVisibility());

        mModel.set(ImprovedBookmarkRowProperties.START_IMAGE_VISIBILITY,
                ImageVisibility.FOLDER_DRAWABLE);
        Assert.assertEquals(View.GONE,
                mImprovedBookmarkRow.findViewById(R.id.start_image_container).getVisibility());
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
        doAnswer((invocation) -> {
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
        mModel.set(ImprovedBookmarkRowProperties.START_ICON_DRAWABLE, mDrawable);

        verify(mStartImageView).setImageDrawable(mDrawable);
        verify(mStartImageView).setAlpha(0f);
        verify(mStartImageView).animate();
        verify(mStartImageViewAnimator).alpha(1f);
        verify(mStartImageViewAnimator).setDuration(ImprovedBookmarkRow.BASE_ANIMATION_DURATION_MS);
        verify(mStartImageViewAnimator).start();
    }

    @Test
    public void testSetStartImageDrawable_nullDrawableDoesNotAnimate() {
        mModel.set(ImprovedBookmarkRowProperties.START_ICON_DRAWABLE, mDrawable);
        mImprovedBookmarkRow.setStartImageViewForTesting(mStartImageView);

        mModel.set(ImprovedBookmarkRowProperties.END_IMAGE_VISIBILITY, ImageVisibility.DRAWABLE);
        mModel.set(ImprovedBookmarkRowProperties.START_ICON_DRAWABLE, null);

        verify(mStartImageView).setImageDrawable(null);
        verify(mStartImageView, never()).setAlpha(0f);
        verify(mStartImageView, never()).animate();
        verify(mStartImageViewAnimator, never()).alpha(1f);
        verify(mStartImageViewAnimator, never())
                .setDuration(ImprovedBookmarkRow.BASE_ANIMATION_DURATION_MS);
        verify(mStartImageViewAnimator, never()).start();
    }
}
