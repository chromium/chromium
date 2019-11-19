// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.base;

import static org.mockito.Mockito.anyInt;
import static org.mockito.Mockito.inOrder;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.view.View;
import android.widget.ImageView;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.InOrder;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;

import org.chromium.chrome.browser.ui.widget.RoundedCornerImageView;
import org.chromium.testing.local.LocalRobolectricTestRunner;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/**
 * Tests for {@link BaseSuggestionViewBinder}.
 */
@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class BaseSuggestionViewBinderUnitTest {
    @Mock
    BaseSuggestionView mBaseView;

    @Mock
    RoundedCornerImageView mDecorView;

    @Mock
    ImageView mActionView;

    private Activity mActivity;

    private PropertyModel mModel;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);

        mActivity = Robolectric.buildActivity(Activity.class).setup().get();

        when(mBaseView.getContext()).thenReturn(mActivity);
        when(mDecorView.getContext()).thenReturn(mActivity);
        when(mActionView.getContext()).thenReturn(mActivity);
        when(mBaseView.getSuggestionImageView()).thenReturn(mDecorView);
        when(mBaseView.getActionImageView()).thenReturn(mActionView);

        mModel = new PropertyModel(BaseSuggestionViewProperties.ALL_KEYS);
        PropertyModelChangeProcessor.create(mModel, mBaseView, new BaseSuggestionViewBinder());
    }

    @Test
    public void decorIcon_showSquareIcon() {
        SuggestionDrawableState state = SuggestionDrawableState.Builder.forColor(0).build();
        mModel.set(BaseSuggestionViewProperties.ICON, state);

        // Expect a single call to setRoundedCorners, and make sure this call sets all radii to 0.
        verify(mDecorView).setRoundedCorners(0, 0, 0, 0);
        verify(mDecorView).setRoundedCorners(anyInt(), anyInt(), anyInt(), anyInt());

        verify(mDecorView).setVisibility(View.VISIBLE);
        verify(mDecorView).setImageDrawable(state.drawable);
    }

    @Test
    public void decorIcon_showRoundedIcon() {
        SuggestionDrawableState state =
                SuggestionDrawableState.Builder.forColor(0).setUseRoundedCorners(true).build();
        mModel.set(BaseSuggestionViewProperties.ICON, state);

        // Expect a single call to setRoundedCorners, and make sure this call sets radii to non-0.
        verify(mDecorView, never()).setRoundedCorners(0, 0, 0, 0);
        verify(mDecorView).setRoundedCorners(anyInt(), anyInt(), anyInt(), anyInt());

        verify(mDecorView).setVisibility(View.VISIBLE);
        verify(mDecorView).setImageDrawable(state.drawable);
    }

    @Test
    public void decorIcon_hideIcon() {
        InOrder ordered = inOrder(mDecorView);

        SuggestionDrawableState state = SuggestionDrawableState.Builder.forColor(0).build();
        mModel.set(BaseSuggestionViewProperties.ICON, state);
        mModel.set(BaseSuggestionViewProperties.ICON, null);

        ordered.verify(mDecorView).setVisibility(View.VISIBLE);
        ordered.verify(mDecorView).setImageDrawable(state.drawable);
        ordered.verify(mDecorView).setVisibility(View.GONE);
        // Ensure we're releasing drawable to free memory.
        ordered.verify(mDecorView).setImageDrawable(null);
    }

    @Test
    public void actionIcon_showSquareIcon() {
        SuggestionDrawableState state = SuggestionDrawableState.Builder.forColor(0).build();
        mModel.set(BaseSuggestionViewProperties.ACTION_ICON, state);

        verify(mActionView).setVisibility(View.VISIBLE);
        verify(mActionView).setImageDrawable(state.drawable);
    }

    @Test
    public void actionIcon_hideIcon() {
        InOrder ordered = inOrder(mActionView);

        SuggestionDrawableState state = SuggestionDrawableState.Builder.forColor(0).build();
        mModel.set(BaseSuggestionViewProperties.ACTION_ICON, state);
        mModel.set(BaseSuggestionViewProperties.ACTION_ICON, null);

        ordered.verify(mActionView).setVisibility(View.VISIBLE);
        ordered.verify(mActionView).setImageDrawable(state.drawable);
        ordered.verify(mActionView).setVisibility(View.GONE);
        // Ensure we're releasing drawable to free memory.
        ordered.verify(mActionView).setImageDrawable(null);
    }
}
