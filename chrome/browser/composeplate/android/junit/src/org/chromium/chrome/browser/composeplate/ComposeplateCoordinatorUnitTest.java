// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.composeplate;

import static org.junit.Assert.assertFalse;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.view.View;
import android.view.ViewGroup;
import android.widget.ImageView;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.util.BrowserUiUtils.ModuleTypeOnStartAndNtp;

/** Unit tests for {@link ComposeplateCoordinator}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class ComposeplateCoordinatorUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private ViewGroup mParentView;
    @Mock private View mComposeplateView;
    @Mock private ImageView mVoiceSearchButton;
    @Mock private ImageView mLensButton;
    @Mock private ImageView mIncognitoButton;
    @Mock private View.OnClickListener mOriginalOnClickListener;

    private ComposeplateCoordinator mCoordinator;

    @Before
    public void setUp() {
        when(mParentView.findViewById(R.id.composeplate_view)).thenReturn(mComposeplateView);
        when(mComposeplateView.findViewById(R.id.voice_search_button))
                .thenReturn(mVoiceSearchButton);
        when(mComposeplateView.findViewById(R.id.lens_camera_button)).thenReturn(mLensButton);
        when(mComposeplateView.findViewById(R.id.incognito_button)).thenReturn(mIncognitoButton);

        mCoordinator = new ComposeplateCoordinator(mParentView);
    }

    @Test
    public void testSetVisibility() {
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        ComposeplateMetricsUtils.HISTOGRAM_COMPOSEPLATE_IMPRESSION, true);
        mCoordinator.setVisibility(/* visible= */ true, /* isCurrentPage= */ true);
        verify(mComposeplateView).setVisibility(View.VISIBLE);
        histogramWatcher.assertExpected();

        histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        ComposeplateMetricsUtils.HISTOGRAM_COMPOSEPLATE_IMPRESSION, false);
        mCoordinator.setVisibility(/* visible= */ false, /* isCurrentPage= */ true);
        verify(mComposeplateView).setVisibility(View.GONE);
        histogramWatcher.assertExpected();
    }

    @Test
    public void testSetIncognitoButtonVisibility() {
        assertFalse(ChromeFeatureList.sAndroidComposeplateHideIncognitoButton.getValue());
        mCoordinator.setVisibility(/* visible= */ true, /* isCurrentPage= */ true);
        verify(mComposeplateView).setVisibility(View.VISIBLE);
        verify(mIncognitoButton).setVisibility(View.VISIBLE);

        mCoordinator.setVisibility(/* visible= */ false, /* isCurrentPage= */ true);
        verify(mComposeplateView).setVisibility(View.GONE);
        verify(mIncognitoButton).setVisibility(View.GONE);
    }

    @Test
    public void testSetIncognitoButtonVisibility_HideIncognitoButton() {
        ChromeFeatureList.sAndroidComposeplateHideIncognitoButton.setForTesting(true);
        mCoordinator = new ComposeplateCoordinator(mParentView);

        mCoordinator.setVisibility(/* visible= */ true, /* isCurrentPage= */ true);
        verify(mComposeplateView).setVisibility(View.VISIBLE);
        verify(mIncognitoButton).setVisibility(View.GONE);

        mCoordinator.setVisibility(/* visible= */ false, /* isCurrentPage= */ true);
        verify(mComposeplateView).setVisibility(View.GONE);
        verify(mIncognitoButton).setVisibility(View.GONE);
    }

    @Test
    public void testSetVoiceSearchClickListener() {
        mCoordinator.setVoiceSearchClickListener(mOriginalOnClickListener);
        View.OnClickListener enhancedListener = getCapturedOnClickListener(mVoiceSearchButton);

        HistogramWatcher histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "NewTabPage.Module.Click",
                        ModuleTypeOnStartAndNtp.COMPOSEPLATE_VIEW_VOICE_SEARCH_BUTTON);

        View clickedView = mock(View.class);
        enhancedListener.onClick(clickedView);

        histogramWatcher.assertExpected();
        verify(mOriginalOnClickListener).onClick(clickedView);
    }

    @Test
    public void testSetLensClickListener() {
        mCoordinator.setLensClickListener(mOriginalOnClickListener);
        View.OnClickListener enhancedListener = getCapturedOnClickListener(mLensButton);

        HistogramWatcher histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "NewTabPage.Module.Click",
                        ModuleTypeOnStartAndNtp.COMPOSEPLATE_VIEW_LENS_BUTTON);

        View clickedView = mock(View.class);
        enhancedListener.onClick(clickedView);

        histogramWatcher.assertExpected();
        verify(mOriginalOnClickListener).onClick(clickedView);
    }

    @Test
    public void testSetIncognitoClickListener() {
        mCoordinator.setIncognitoClickListener(mOriginalOnClickListener);
        View.OnClickListener enhancedListener = getCapturedOnClickListener(mIncognitoButton);

        HistogramWatcher histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "NewTabPage.Module.Click",
                        ModuleTypeOnStartAndNtp.COMPOSEPLATE_VIEW_INCOGNITO_BUTTON);

        View clickedView = mock(View.class);
        enhancedListener.onClick(clickedView);

        histogramWatcher.assertExpected();
        verify(mOriginalOnClickListener).onClick(clickedView);
    }

    private View.OnClickListener getCapturedOnClickListener(ImageView button) {
        ArgumentCaptor<View.OnClickListener> listenerCaptor =
                ArgumentCaptor.forClass(View.OnClickListener.class);
        verify(button).setOnClickListener(listenerCaptor.capture());
        return listenerCaptor.getValue();
    }
}
