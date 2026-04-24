// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp.search;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.content.Context;
import android.content.res.ColorStateList;
import android.graphics.Color;
import android.graphics.Point;
import android.graphics.Rect;
import android.graphics.drawable.ColorDrawable;
import android.graphics.drawable.Drawable;
import android.graphics.drawable.GradientDrawable;
import android.text.TextWatcher;
import android.view.ContextThemeWrapper;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;

import androidx.appcompat.content.res.AppCompatResources;
import androidx.test.core.app.ApplicationProvider;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.composeplate.ComposeplateUtils;
import org.chromium.chrome.browser.feed.FeedSurfaceScrollDelegate;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.omnibox.status.StatusProperties.StatusIconResource;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.function.Supplier;

/** Unit tests for {@link SearchBoxMediator}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class SearchBoxMediatorUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private ActivityLifecycleDispatcher mActivityLifecycleDispatcher;
    @Mock private View.OnClickListener mLensClickListener;
    @Mock private View.OnClickListener mVoiceSearchClickListener;
    @Mock private FeedSurfaceScrollDelegate mScrollDelegate;
    @Mock private Supplier<Integer> mTabStripHeightSupplier;

    private Context mContext;
    private ViewGroup mView;
    private PropertyModel mPropertyModel;
    private SearchBoxMediator mMediator;

    @Before
    public void setup() {
        mContext =
                new ContextThemeWrapper(
                        ApplicationProvider.getApplicationContext(),
                        R.style.Theme_BrowserUI_DayNight);
        mView =
                spy(
                        (ViewGroup)
                                LayoutInflater.from(mContext)
                                        .inflate(R.layout.fake_search_box_layout, null));

        mPropertyModel = new PropertyModel.Builder(SearchBoxProperties.ALL_KEYS).build();
        mMediator =
                new SearchBoxMediator(
                        mContext,
                        mPropertyModel,
                        mView,
                        /* isTablet= */ false,
                        mActivityLifecycleDispatcher);
    }

    @Test
    public void testOnDestroy() {
        Drawable voiceSearchDrawable =
                AppCompatResources.getDrawable(mContext, R.drawable.ic_mic_white_24dp);

        mPropertyModel.set(SearchBoxProperties.LENS_CLICK_CALLBACK, mLensClickListener);
        mPropertyModel.set(
                SearchBoxProperties.VOICE_SEARCH_CLICK_CALLBACK, mVoiceSearchClickListener);
        mPropertyModel.set(SearchBoxProperties.VOICE_SEARCH_DRAWABLE, voiceSearchDrawable);
        mPropertyModel.set(
                SearchBoxProperties.SEARCH_BOX_CLICK_CALLBACK, mock(View.OnClickListener.class));
        mPropertyModel.set(
                SearchBoxProperties.SEARCH_BOX_DRAG_CALLBACK, mock(View.OnDragListener.class));
        mPropertyModel.set(SearchBoxProperties.SEARCH_BOX_TEXT_WATCHER, mock(TextWatcher.class));
        mPropertyModel.set(SearchBoxProperties.DSE_ICON_DRAWABLE, new ColorDrawable(Color.RED));

        assertNotNull(mPropertyModel.get(SearchBoxProperties.LENS_CLICK_CALLBACK));
        assertNotNull(mPropertyModel.get(SearchBoxProperties.VOICE_SEARCH_CLICK_CALLBACK));
        assertNotNull(mPropertyModel.get(SearchBoxProperties.VOICE_SEARCH_DRAWABLE));
        assertNotNull(mPropertyModel.get(SearchBoxProperties.SEARCH_BOX_CLICK_CALLBACK));
        assertNotNull(mPropertyModel.get(SearchBoxProperties.SEARCH_BOX_DRAG_CALLBACK));
        assertNotNull(mPropertyModel.get(SearchBoxProperties.SEARCH_BOX_TEXT_WATCHER));
        assertNotNull(mPropertyModel.get(SearchBoxProperties.DSE_ICON_DRAWABLE));

        mMediator.onDestroy();

        verify(mActivityLifecycleDispatcher).unregister(mMediator);
        assertNull(mPropertyModel.get(SearchBoxProperties.LENS_CLICK_CALLBACK));
        assertNull(mPropertyModel.get(SearchBoxProperties.VOICE_SEARCH_CLICK_CALLBACK));
        assertNull(mPropertyModel.get(SearchBoxProperties.VOICE_SEARCH_DRAWABLE));

        assertNull(mPropertyModel.get(SearchBoxProperties.SEARCH_BOX_CLICK_CALLBACK));
        assertNull(mPropertyModel.get(SearchBoxProperties.SEARCH_BOX_DRAG_CALLBACK));
        assertNull(mPropertyModel.get(SearchBoxProperties.SEARCH_BOX_TEXT_WATCHER));
        assertNull(mPropertyModel.get(SearchBoxProperties.DSE_ICON_DRAWABLE));
    }

    @Test
    public void testSetSearchEngineIcon() {
        Drawable drawable = new ColorDrawable(Color.RED);
        StatusIconResource newIcon = new StatusIconResource(drawable);
        mMediator.setSearchEngineIcon(newIcon);
        assertEquals(drawable, mPropertyModel.get(SearchBoxProperties.DSE_ICON_DRAWABLE));
    }

    @Test
    public void testSetSearchEngineIcon_Google() {
        StatusIconResource googleIcon = new StatusIconResource(R.drawable.ic_logo_googleg_20dp, 0);
        mMediator.setSearchEngineIcon(googleIcon);
        assertEquals(
                R.drawable.ic_logo_googleg_24dp,
                mPropertyModel.get(SearchBoxProperties.DSE_ICON_RESOURCE_ID));
    }

    @Test
    public void testSetSearchEngineIcon_Null() {
        mMediator.setSearchEngineIcon(null);
        assertEquals(
                R.drawable.ic_search_24dp,
                mPropertyModel.get(SearchBoxProperties.DSE_ICON_RESOURCE_ID));
    }

    @Test
    public void testSetEndPadding() {
        int padding = 10;
        mMediator.setEndPadding(padding);
        assertEquals(padding, mPropertyModel.get(SearchBoxProperties.SEARCH_BOX_END_PADDING));
    }

    @Test
    public void testSetSearchBoxTextAppearance() {
        int resId = R.style.TextAppearance_FakeSearchBoxTextMedium;
        mMediator.setSearchBoxTextAppearance(resId);
        assertEquals(resId, mPropertyModel.get(SearchBoxProperties.SEARCH_BOX_TEXT_STYLE_RES_ID));
    }

    @Test
    public void testEnableSearchBoxEditText() {
        mMediator.enableSearchBoxEditText(true);
        assertTrue(mPropertyModel.get(SearchBoxProperties.ENABLE_SEARCH_BOX_EDIT_TEXT));

        mMediator.enableSearchBoxEditText(false);
        assertFalse(mPropertyModel.get(SearchBoxProperties.ENABLE_SEARCH_BOX_EDIT_TEXT));
    }

    @Test
    public void testSetSearchBoxHintText() {
        String hint = "new hint";
        mMediator.setSearchBoxHintText(hint);
        assertEquals(hint, mPropertyModel.get(SearchBoxProperties.SEARCH_BOX_HINT_TEXT));
    }

    @Test
    public void testApplyWhiteBackgroundWithShadow() {
        Drawable defaultBackground =
                mContext.getDrawable(R.drawable.home_surface_search_box_background);
        View searchBoxContainer = mView.findViewById(R.id.search_box_container);

        // Tests the case to apply a white background with shadow.
        int resId = R.style.TextAppearance_FakeSearchBoxTextMediumDark;
        ColorStateList colorStateList =
                ComposeplateUtils.getSearchBoxIconColorTint(
                        mContext, /* shouldApplyWhiteBackgroundOnSearchBox= */ true);
        mMediator.applyWhiteBackground(true);
        assertTrue(mPropertyModel.get(SearchBoxProperties.APPLY_WHITE_BACKGROUND));
        assertEquals(resId, mPropertyModel.get(SearchBoxProperties.SEARCH_BOX_TEXT_STYLE_RES_ID));
        assertEquals(
                colorStateList,
                mPropertyModel.get(SearchBoxProperties.VOICE_SEARCH_COLOR_STATE_LIST));
        verifyApplyBackground(searchBoxContainer);

        // Tests the case to remove the white background with shadow.
        resId = R.style.TextAppearance_FakeSearchBoxTextMedium;
        colorStateList =
                ComposeplateUtils.getSearchBoxIconColorTint(
                        mContext, /* shouldApplyWhiteBackgroundOnSearchBox= */ false);
        mMediator.applyWhiteBackground(false);
        assertFalse(mPropertyModel.get(SearchBoxProperties.APPLY_WHITE_BACKGROUND));
        assertEquals(resId, mPropertyModel.get(SearchBoxProperties.SEARCH_BOX_TEXT_STYLE_RES_ID));
        assertEquals(
                colorStateList,
                mPropertyModel.get(SearchBoxProperties.VOICE_SEARCH_COLOR_STATE_LIST));
        verifyResetBackground(searchBoxContainer, defaultBackground);
    }

    @Test
    public void testIsSearchBoxOffscreen() {
        // Mock scroll view not initialized.
        when(mScrollDelegate.isScrollViewInitialized()).thenReturn(false);
        assertFalse(mMediator.isSearchBoxOffscreen(mScrollDelegate));

        // Mock scroll view initialized.
        when(mScrollDelegate.isScrollViewInitialized()).thenReturn(true);

        // Mock first child not visible.
        when(mScrollDelegate.isChildVisibleAtPosition(0)).thenReturn(false);
        assertTrue(mMediator.isSearchBoxOffscreen(mScrollDelegate));

        // Mock first child visible, but scroll offset is large.
        when(mScrollDelegate.isChildVisibleAtPosition(0)).thenReturn(true);
        int searchBoxTop = 100;
        mView.setTop(searchBoxTop);
        int transitionEndOffset =
                mContext.getResources()
                        .getDimensionPixelSize(R.dimen.ntp_search_box_transition_end_offset);
        int scrollY = searchBoxTop + transitionEndOffset + 1;
        when(mScrollDelegate.getVerticalScrollOffset()).thenReturn(scrollY);
        assertTrue(mMediator.isSearchBoxOffscreen(mScrollDelegate));

        // Mock first child visible, scroll offset is small.
        scrollY = searchBoxTop + transitionEndOffset;
        when(mScrollDelegate.getVerticalScrollOffset()).thenReturn(scrollY);
        assertFalse(mMediator.isSearchBoxOffscreen(mScrollDelegate));
    }

    @Test
    public void testGetToolbarTransitionPercentage_NotInitialized() {
        // Sets the scroll delegate hasn't been initialized.
        when(mScrollDelegate.isScrollViewInitialized()).thenReturn(false);
        int transitionStartOffset = 30;
        float expectedPercentage = 0f;
        assertEquals(
                expectedPercentage,
                mMediator.getToolbarTransitionPercentage(
                        mScrollDelegate, mTabStripHeightSupplier, transitionStartOffset),
                0.01f);

        // When the view isn't fully initialized at startup, mView.getTop() returns 0.
        mView.setTop(0);
        when(mScrollDelegate.isScrollViewInitialized()).thenReturn(true);
        when(mScrollDelegate.isChildVisibleAtPosition(0)).thenReturn(true);
        when(mScrollDelegate.getVerticalScrollOffset()).thenReturn(0);

        assertEquals(
                expectedPercentage,
                mMediator.getToolbarTransitionPercentage(
                        mScrollDelegate, mTabStripHeightSupplier, transitionStartOffset),
                0.01f);
    }

    @Test
    public void testGetToolbarTransitionPercentage_Offscreen() {
        when(mScrollDelegate.isScrollViewInitialized()).thenReturn(true);
        when(mScrollDelegate.isChildVisibleAtPosition(0)).thenReturn(false);
        int transitionStartOffset = 30;

        float expectedPercentage = 1f;
        assertEquals(
                expectedPercentage,
                mMediator.getToolbarTransitionPercentage(
                        mScrollDelegate, mTabStripHeightSupplier, transitionStartOffset),
                0.01f);
    }

    @Test
    public void testGetToolbarTransitionPercentage() {
        int searchBoxTop = 100;
        int searchBoxPaddingTop = 10;
        int transitionStartOffset = 50;
        int tabStripHeight = 30;
        int transitionEndOffset =
                mContext.getResources()
                        .getDimensionPixelSize(R.dimen.ntp_search_box_transition_end_offset);
        mView.setTop(searchBoxTop);
        mView.setPadding(0, searchBoxPaddingTop, 0, 0);

        when(mScrollDelegate.isScrollViewInitialized()).thenReturn(true);
        when(mScrollDelegate.isChildVisibleAtPosition(0)).thenReturn(true);
        when(mTabStripHeightSupplier.get()).thenReturn(tabStripHeight);

        // Transition starts when scrollY = searchBoxTop + paddingTop - transitionStartOffset -
        // tabStripHeight.
        int scrollY = (searchBoxTop + searchBoxPaddingTop) - transitionStartOffset - tabStripHeight;
        when(mScrollDelegate.getVerticalScrollOffset()).thenReturn(scrollY);
        float expectedPercentage = 0f;
        assertEquals(
                expectedPercentage,
                mMediator.getToolbarTransitionPercentage(
                        mScrollDelegate, mTabStripHeightSupplier, transitionStartOffset),
                0.01f);

        // Transition ends when scrollY = searchBoxTop + paddingTop + transitionEndOffset -
        // tabStripHeight.
        scrollY = searchBoxTop + searchBoxPaddingTop + transitionEndOffset - tabStripHeight;
        when(mScrollDelegate.getVerticalScrollOffset()).thenReturn(scrollY);
        expectedPercentage = 1f;
        assertEquals(
                expectedPercentage,
                mMediator.getToolbarTransitionPercentage(
                        mScrollDelegate, mTabStripHeightSupplier, transitionStartOffset),
                0.01f);

        // Transition in the middle.
        scrollY =
                searchBoxTop
                        + searchBoxPaddingTop
                        + transitionEndOffset
                        - tabStripHeight
                        - (transitionStartOffset + transitionEndOffset) / 2;
        when(mScrollDelegate.getVerticalScrollOffset()).thenReturn(scrollY);
        expectedPercentage = 0.5f;
        assertEquals(
                expectedPercentage,
                mMediator.getToolbarTransitionPercentage(
                        mScrollDelegate, mTabStripHeightSupplier, transitionStartOffset),
                0.01f);
    }

    @Test
    public void testGetToolbarTransitionPercentage_Tablet() {
        SearchBoxMediator tabletMediator =
                new SearchBoxMediator(
                        mContext,
                        mPropertyModel,
                        mView,
                        /* isTablet= */ true,
                        mActivityLifecycleDispatcher);
        int searchBoxTop = 100;
        int searchBoxPaddingTop = 10;
        int transitionStartOffset = 50;
        int tabStripHeight = 30;
        mView.setTop(searchBoxTop);
        mView.setPadding(0, searchBoxPaddingTop, 0, 0);

        when(mScrollDelegate.isScrollViewInitialized()).thenReturn(true);
        when(mScrollDelegate.isChildVisibleAtPosition(0)).thenReturn(true);
        when(mTabStripHeightSupplier.get()).thenReturn(tabStripHeight);

        // Transition starts when scrollY = searchBoxTop + paddingTop - transitionStartOffset -
        // tabStripHeight.
        int scrollY = (searchBoxTop + searchBoxPaddingTop) - transitionStartOffset - tabStripHeight;
        when(mScrollDelegate.getVerticalScrollOffset()).thenReturn(scrollY);
        float expectedPercentage = 0f;
        assertEquals(
                expectedPercentage,
                tabletMediator.getToolbarTransitionPercentage(
                        mScrollDelegate, mTabStripHeightSupplier, transitionStartOffset),
                0.01f);

        // On tablet transitionEndOffset is 0.
        // Transition ends when scrollY = searchBoxTop + paddingTop - tabStripHeight.
        scrollY = searchBoxTop + searchBoxPaddingTop - tabStripHeight;
        when(mScrollDelegate.getVerticalScrollOffset()).thenReturn(scrollY);
        expectedPercentage = 1f;
        assertEquals(
                expectedPercentage,
                tabletMediator.getToolbarTransitionPercentage(
                        mScrollDelegate, mTabStripHeightSupplier, transitionStartOffset),
                0.01f);
    }

    @Test
    public void testGetSearchBoxBounds() {
        Rect bounds = new Rect();
        Point translation = new Point();
        ViewGroup rootView = mock(ViewGroup.class);
        int verticalInset = 5;

        int searchBoxLeft = 10;
        int searchBoxTop = 20;
        int searchBoxWidth = 100;
        int searchBoxHeight = 40;
        mView.setLeft(searchBoxLeft);
        mView.setTop(searchBoxTop);
        mView.setRight(searchBoxLeft + searchBoxWidth);
        mView.setBottom(searchBoxTop + searchBoxHeight);

        // Mock parent hierarchy
        ViewGroup parentView = mock(ViewGroup.class);
        doReturn(parentView).when(mView).getParent();
        doReturn(rootView).when(parentView).getParent();

        int parentX = 5;
        int parentY = 10;
        when(parentView.getX()).thenReturn((float) parentX);
        when(parentView.getY()).thenReturn((float) parentY);
        when(parentView.getScrollX()).thenReturn(0);
        when(parentView.getScrollY()).thenReturn(0);

        when(rootView.getScrollX()).thenReturn(0);
        when(rootView.getScrollY()).thenReturn(0);

        when(mScrollDelegate.isScrollViewInitialized()).thenReturn(true);
        when(mScrollDelegate.isChildVisibleAtPosition(0)).thenReturn(true);
        // Ensure scrollY is not large enough to trigger offscreen.
        when(mScrollDelegate.getVerticalScrollOffset()).thenReturn(0);

        mMediator.getSearchBoxBounds(bounds, translation, rootView, mScrollDelegate, verticalInset);

        int expectedTranslationX = parentX;
        int expectedTranslationY = parentY;
        assertEquals(new Point(expectedTranslationX, expectedTranslationY), translation);

        Rect expectedBounds =
                new Rect(
                        searchBoxLeft + expectedTranslationX,
                        searchBoxTop + expectedTranslationY,
                        searchBoxLeft + searchBoxWidth + expectedTranslationX,
                        searchBoxTop + searchBoxHeight + expectedTranslationY);
        expectedBounds.inset(0, verticalInset);
        assertEquals(expectedBounds, bounds);
    }

    private void verifyApplyBackground(View view) {
        // Verifies that the background is set to color white.
        Drawable whiteBackground = view.getBackground();
        assertTrue(whiteBackground instanceof GradientDrawable);
        assertEquals(
                Color.WHITE, ((GradientDrawable) whiteBackground).getColor().getDefaultColor());
    }

    private void verifyResetBackground(View view, Drawable defaultBackground) {
        // Verifies that the background of the view is to reset.
        assertEquals(
                ((GradientDrawable) defaultBackground).getColor().getDefaultColor(),
                ((GradientDrawable) view.getBackground()).getColor().getDefaultColor());
    }
}
