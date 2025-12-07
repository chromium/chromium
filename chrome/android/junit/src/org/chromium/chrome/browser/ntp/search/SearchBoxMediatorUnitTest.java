// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp.search;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.verify;

import android.content.Context;
import android.graphics.Color;
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
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.ui.modelutil.PropertyModel;

/** Unit tests for {@link SearchBoxMediator}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class SearchBoxMediatorUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private ActivityLifecycleDispatcher mActivityLifecycleDispatcher;
    @Mock private View.OnClickListener mLensClickListener;
    @Mock private View.OnClickListener mVoiceSearchClickListener;
    @Mock private View.OnClickListener mComposePlateClickListener;

    private Context mContext;
    private ViewGroup mView;
    private PropertyModel mPropertyModel;
    private Drawable mVoiceSearchDrawable;
    private SearchBoxMediator mMediator;

    @Before
    public void setup() {
        mContext =
                new ContextThemeWrapper(
                        ApplicationProvider.getApplicationContext(),
                        R.style.Theme_BrowserUI_DayNight);
        mView =
                (ViewGroup)
                        LayoutInflater.from(mContext)
                                .inflate(R.layout.fake_search_box_layout, null);

        mPropertyModel = new PropertyModel.Builder(SearchBoxProperties.ALL_KEYS).build();
        mMediator = new SearchBoxMediator(mContext, mPropertyModel, mView);
    }

    @Test
    public void testOnDestroy() {
        mVoiceSearchDrawable =
                AppCompatResources.getDrawable(mContext, R.drawable.ic_mic_white_24dp);

        mPropertyModel.set(SearchBoxProperties.LENS_CLICK_CALLBACK, mLensClickListener);
        mPropertyModel.set(
                SearchBoxProperties.VOICE_SEARCH_CLICK_CALLBACK, mVoiceSearchClickListener);
        mPropertyModel.set(
                SearchBoxProperties.COMPOSEPLATE_BUTTON_CLICK_CALLBACK, mComposePlateClickListener);
        mPropertyModel.set(SearchBoxProperties.VOICE_SEARCH_DRAWABLE, mVoiceSearchDrawable);
        mPropertyModel.set(
                SearchBoxProperties.SEARCH_BOX_CLICK_CALLBACK, mock(View.OnClickListener.class));
        mPropertyModel.set(
                SearchBoxProperties.SEARCH_BOX_DRAG_CALLBACK, mock(View.OnDragListener.class));
        mPropertyModel.set(SearchBoxProperties.SEARCH_BOX_TEXT_WATCHER, mock(TextWatcher.class));

        assertNotNull(mPropertyModel.get(SearchBoxProperties.LENS_CLICK_CALLBACK));
        assertNotNull(mPropertyModel.get(SearchBoxProperties.VOICE_SEARCH_CLICK_CALLBACK));
        assertNotNull(mPropertyModel.get(SearchBoxProperties.COMPOSEPLATE_BUTTON_CLICK_CALLBACK));
        assertNotNull(mPropertyModel.get(SearchBoxProperties.VOICE_SEARCH_DRAWABLE));
        assertNotNull(mPropertyModel.get(SearchBoxProperties.SEARCH_BOX_CLICK_CALLBACK));
        assertNotNull(mPropertyModel.get(SearchBoxProperties.SEARCH_BOX_DRAG_CALLBACK));
        assertNotNull(mPropertyModel.get(SearchBoxProperties.SEARCH_BOX_TEXT_WATCHER));

        mMediator.initialize(mActivityLifecycleDispatcher);
        mMediator.onDestroy();

        verify(mActivityLifecycleDispatcher).unregister(mMediator);
        assertNull(mPropertyModel.get(SearchBoxProperties.LENS_CLICK_CALLBACK));
        assertNull(mPropertyModel.get(SearchBoxProperties.VOICE_SEARCH_CLICK_CALLBACK));
        assertNull(mPropertyModel.get(SearchBoxProperties.COMPOSEPLATE_BUTTON_CLICK_CALLBACK));
        assertNull(mPropertyModel.get(SearchBoxProperties.VOICE_SEARCH_DRAWABLE));

        assertNull(mPropertyModel.get(SearchBoxProperties.SEARCH_BOX_CLICK_CALLBACK));
        assertNull(mPropertyModel.get(SearchBoxProperties.SEARCH_BOX_DRAG_CALLBACK));
        assertNull(mPropertyModel.get(SearchBoxProperties.SEARCH_BOX_TEXT_WATCHER));
    }

    @Test
    public void testSetEndPadding() {
        int padding = 10;
        mMediator.setEndPadding(padding);
        assertEquals(padding, mPropertyModel.get(SearchBoxProperties.SEARCH_BOX_END_PADDING));
    }

    @Test
    public void testSetStartPadding() {
        int padding = 20;
        mMediator.setStartPadding(padding);
        assertEquals(padding, mPropertyModel.get(SearchBoxProperties.SEARCH_BOX_START_PADDING));
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
        float expectedElevation =
                mContext.getResources().getDimensionPixelSize(R.dimen.ntp_search_box_elevation);
        assertNotEquals(0, Float.compare(0f, expectedElevation));
        Drawable defaultBackground =
                mContext.getDrawable(R.drawable.home_surface_search_box_background);
        View searchBoxContainer = mView.findViewById(R.id.search_box_container);

        // Tests the case to apply a white background with shadow.
        mMediator.applyWhiteBackgroundWithShadow(true);
        assertTrue(mPropertyModel.get(SearchBoxProperties.APPLY_WHITE_BACKGROUND_WITH_SHADOW));
        assertEquals(0, Float.compare(expectedElevation, mView.getElevation()));
        assertTrue(mView.getClipToOutline());
        // Verifies that the search_box_container's background is set to color white.
        Drawable whiteBackground = searchBoxContainer.getBackground();
        assertTrue(whiteBackground instanceof GradientDrawable whiteGradientDrawable);
        assertEquals(
                Color.WHITE, ((GradientDrawable) whiteBackground).getColor().getDefaultColor());

        // Tests the case to remove the white background with shadow.
        mMediator.applyWhiteBackgroundWithShadow(false);
        assertFalse(mPropertyModel.get(SearchBoxProperties.APPLY_WHITE_BACKGROUND_WITH_SHADOW));
        assertEquals(0, Float.compare(0f, mView.getElevation()));
        assertFalse(mView.getClipToOutline());
        assertNull(mView.getBackground());
        // Verifies that the background of the search_box_container is to reset to the default one.
        assertEquals(
                ((GradientDrawable) defaultBackground).getColor().getDefaultColor(),
                ((GradientDrawable) searchBoxContainer.getBackground())
                        .getColor()
                        .getDefaultColor());
    }
}
