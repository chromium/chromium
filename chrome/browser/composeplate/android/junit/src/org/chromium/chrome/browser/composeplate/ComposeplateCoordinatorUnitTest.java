// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.composeplate;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.content.Context;
import android.content.res.ColorStateList;
import android.view.ContextThemeWrapper;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ImageView;

import androidx.annotation.StyleRes;
import androidx.test.core.app.ApplicationProvider;

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
import org.chromium.chrome.browser.incognito.IncognitoUtils;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.util.BrowserUiUtils.ModuleTypeOnStartAndNtp;
import org.chromium.ui.modelutil.PropertyModel;

/** Unit tests for {@link ComposeplateCoordinator}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class ComposeplateCoordinatorUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private ViewGroup mParentView;
    @Mock private ComposeplateView mComposeplateView;
    @Mock private ImageView mVoiceSearchButton;
    @Mock private ImageView mLensButton;
    @Mock private View mIncognitoButton;
    @Mock private View mComposeplateButton;
    @Mock private View.OnClickListener mOriginalOnClickListener;
    @Mock private Profile mProfile;
    @Mock private ColorStateList mColorStateList;

    private Context mContext;
    private ComposeplateCoordinator mCoordinator;
    private PropertyModel mPropertyModel;
    private @StyleRes int mTextStyleResId;

    @Before
    public void setUp() {
        mContext =
                new ContextThemeWrapper(
                        ApplicationProvider.getApplicationContext(),
                        R.style.Theme_BrowserUI_DayNight);
        IncognitoUtils.setEnabledForTesting(true);
        assertTrue(IncognitoUtils.isIncognitoModeEnabled(mProfile));

        when(mParentView.findViewById(R.id.composeplate_view)).thenReturn(mComposeplateView);
        when(mParentView.getResources()).thenReturn(mContext.getResources());
        when(mComposeplateView.findViewById(R.id.voice_search_button))
                .thenReturn(mVoiceSearchButton);
        when(mComposeplateView.findViewById(R.id.lens_camera_button)).thenReturn(mLensButton);
        when(mComposeplateView.findViewById(R.id.incognito_button)).thenReturn(mIncognitoButton);
        when(mComposeplateView.findViewById(R.id.composeplate_button))
                .thenReturn(mComposeplateButton);

        mTextStyleResId = R.style.TextAppearance_ComposeplateTextMedium;
        mCoordinator =
                new ComposeplateCoordinator(
                        mParentView, mProfile, mColorStateList, mTextStyleResId);
        mPropertyModel = mCoordinator.getModelForTesting();
    }

    @Test
    public void testSetVisibilityV1() {
        HistogramWatcher histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        ComposeplateMetricsUtils.HISTOGRAM_COMPOSEPLATE_IMPRESSION, true);
        mCoordinator.setVisibilityV1(/* visible= */ true, /* isCurrentPage= */ true);
        verify(mComposeplateView).setVisibility(View.VISIBLE);
        histogramWatcher.assertExpected();

        histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        ComposeplateMetricsUtils.HISTOGRAM_COMPOSEPLATE_IMPRESSION, false);
        mCoordinator.setVisibilityV1(/* visible= */ false, /* isCurrentPage= */ true);
        verify(mComposeplateView).setVisibility(View.GONE);
        histogramWatcher.assertExpected();
    }

    @Test
    public void testCreate() {
        assertEquals(mColorStateList, mPropertyModel.get(ComposeplateProperties.COLOR_STATE_LIST));
        assertEquals(mTextStyleResId, mPropertyModel.get(ComposeplateProperties.TEXT_STYLE_RES_ID));
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
    public void testSetIncognitoButtonVisibilityV1() {
        assertFalse(ChromeFeatureList.sAndroidComposeplateHideIncognitoButton.getValue());
        mCoordinator.setVisibilityV1(/* visible= */ true, /* isCurrentPage= */ true);
        verify(mComposeplateView).setVisibility(View.VISIBLE);
        verify(mIncognitoButton).setVisibility(View.VISIBLE);

        mCoordinator.setVisibilityV1(/* visible= */ false, /* isCurrentPage= */ true);
        verify(mComposeplateView).setVisibility(View.GONE);
        verify(mIncognitoButton).setVisibility(View.GONE);
    }

    @Test
    public void testSetIncognitoButtonVisibilityV1_HideIncognitoButton() {
        ChromeFeatureList.sAndroidComposeplateHideIncognitoButton.setForTesting(true);
        mCoordinator =
                new ComposeplateCoordinator(
                        mParentView, mProfile, mColorStateList, mTextStyleResId);

        mCoordinator.setVisibilityV1(/* visible= */ true, /* isCurrentPage= */ true);
        verify(mComposeplateView).setVisibility(View.VISIBLE);
        verify(mIncognitoButton).setVisibility(View.GONE);

        mCoordinator.setVisibilityV1(/* visible= */ false, /* isCurrentPage= */ true);
        verify(mComposeplateView).setVisibility(View.GONE);
        verify(mIncognitoButton).setVisibility(View.GONE);
    }

    @Test
    public void testSetIncognitoButtonVisibilityV1_IncognitoDisabled() {
        IncognitoUtils.setEnabledForTesting(false);
        assertFalse(IncognitoUtils.isIncognitoModeEnabled(mProfile));
        assertFalse(ChromeFeatureList.sAndroidComposeplateHideIncognitoButton.getValue());
        mCoordinator =
                new ComposeplateCoordinator(
                        mParentView, mProfile, mColorStateList, mTextStyleResId);

        mCoordinator.setVisibilityV1(/* visible= */ true, /* isCurrentPage= */ true);
        verify(mComposeplateView).setVisibility(View.VISIBLE);
        verify(mIncognitoButton).setVisibility(View.GONE);

        mCoordinator.setVisibilityV1(/* visible= */ false, /* isCurrentPage= */ true);
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

    @Test
    public void testComposeplateButtonClickListener() {
        mCoordinator.setComposeplateButtonClickListener(mOriginalOnClickListener);
        View.OnClickListener enhancedListener = getCapturedOnClickListener(mComposeplateButton);

        HistogramWatcher histogramWatcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "NewTabPage.Module.Click", ModuleTypeOnStartAndNtp.COMPOSEPLATE_BUTTON);

        View clickedView = mock(View.class);
        enhancedListener.onClick(clickedView);

        histogramWatcher.assertExpected();
        verify(mOriginalOnClickListener).onClick(clickedView);
    }

    @Test
    public void testDestroy() {
        mCoordinator.setVoiceSearchClickListener(mOriginalOnClickListener);
        mCoordinator.setLensClickListener(mOriginalOnClickListener);
        mCoordinator.setIncognitoClickListener(mOriginalOnClickListener);
        mCoordinator.setComposeplateButtonClickListener(mOriginalOnClickListener);

        assertNotNull(mPropertyModel.get(ComposeplateProperties.VOICE_SEARCH_CLICK_LISTENER));
        assertNotNull(mPropertyModel.get(ComposeplateProperties.LENS_CLICK_LISTENER));
        assertNotNull(mPropertyModel.get(ComposeplateProperties.INCOGNITO_CLICK_LISTENER));
        assertNotNull(
                mPropertyModel.get(ComposeplateProperties.COMPOSEPLATE_BUTTON_CLICK_LISTENER));

        mCoordinator.destroy();
        assertNull(mPropertyModel.get(ComposeplateProperties.VOICE_SEARCH_CLICK_LISTENER));
        assertNull(mPropertyModel.get(ComposeplateProperties.LENS_CLICK_LISTENER));
        assertNull(mPropertyModel.get(ComposeplateProperties.INCOGNITO_CLICK_LISTENER));
        assertNull(mPropertyModel.get(ComposeplateProperties.COMPOSEPLATE_BUTTON_CLICK_LISTENER));
    }

    @Test
    public void testApplyWhiteBackgroundWithShadow() {
        // Tests the case to apply a white background with shadow.
        mCoordinator.applyWhiteBackgroundWithShadow(true);
        assertTrue(mPropertyModel.get(ComposeplateProperties.APPLY_WHITE_BACKGROUND_WITH_SHADOW));
        verify(mComposeplateView).applyWhiteBackgroundWithShadow(eq(true));

        // Tests the case to remove the white background with shadow.
        mCoordinator.applyWhiteBackgroundWithShadow(false);
        assertFalse(mPropertyModel.get(ComposeplateProperties.APPLY_WHITE_BACKGROUND_WITH_SHADOW));
        verify(mComposeplateView).applyWhiteBackgroundWithShadow(eq(false));
    }

    private View.OnClickListener getCapturedOnClickListener(View button) {
        ArgumentCaptor<View.OnClickListener> listenerCaptor =
                ArgumentCaptor.forClass(View.OnClickListener.class);
        verify(button).setOnClickListener(listenerCaptor.capture());
        return listenerCaptor.getValue();
    }
}
