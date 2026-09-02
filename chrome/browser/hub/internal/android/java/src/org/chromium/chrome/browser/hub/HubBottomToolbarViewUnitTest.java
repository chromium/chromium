// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.hub;

import static android.view.View.GONE;
import static android.view.View.VISIBLE;

import static org.junit.Assert.assertEquals;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import static org.chromium.chrome.browser.hub.HubBottomToolbarProperties.BOTTOM_TOOLBAR_VISIBLE;
import static org.chromium.chrome.browser.hub.HubBottomToolbarProperties.COLOR_SCHEME;
import static org.chromium.chrome.browser.hub.HubColorMixer.COLOR_MIXER;

import android.app.Activity;
import android.graphics.drawable.ColorDrawable;
import android.view.LayoutInflater;

import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;
import org.robolectric.android.controller.ActivityController;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.R;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/** Unit tests for {@link HubBottomToolbarView} and {@link HubBottomToolbarViewBinder}. */
@RunWith(BaseRobolectricTestRunner.class)
public class HubBottomToolbarViewUnitTest {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private HubColorMixer mHubColorMixer;
    @Captor private ArgumentCaptor<HubViewColorBlend> mBlendCaptor;

    private ActivityController<TestActivity> mActivityController;
    private Activity mActivity;
    private HubBottomToolbarView mBottomToolbarView;
    private PropertyModel mPropertyModel;

    @Before
    public void setUp() {
        mActivityController = Robolectric.buildActivity(TestActivity.class).setup();
        mActivity = mActivityController.get();
        mActivity.setTheme(R.style.Theme_BrowserUI_DayNight);
        mBottomToolbarView =
                (HubBottomToolbarView)
                        LayoutInflater.from(mActivity)
                                .inflate(R.layout.hub_bottom_toolbar_layout, null, false);

        mPropertyModel =
                new PropertyModel.Builder(HubBottomToolbarProperties.ALL_BOTTOM_KEYS)
                        .with(COLOR_MIXER, mHubColorMixer)
                        .with(BOTTOM_TOOLBAR_VISIBLE, false)
                        .with(COLOR_SCHEME, HubColorScheme.DEFAULT)
                        .build();

        PropertyModelChangeProcessor.create(
                mPropertyModel, mBottomToolbarView, HubBottomToolbarViewBinder::bind);

        mActivity.setContentView(mBottomToolbarView);
    }

    @After
    public void tearDown() {
        mActivityController.close();
    }

    @Test
    @SmallTest
    public void testVisibilityToggle() {
        // Initial state should be GONE
        assertEquals(GONE, mBottomToolbarView.getVisibility());

        // Test setting visibility to true
        mPropertyModel.set(BOTTOM_TOOLBAR_VISIBLE, true);
        assertEquals(VISIBLE, mBottomToolbarView.getVisibility());

        // Test setting visibility to false
        mPropertyModel.set(BOTTOM_TOOLBAR_VISIBLE, false);
        assertEquals(GONE, mBottomToolbarView.getVisibility());
    }

    @Test
    @SmallTest
    public void testColorScheme() {
        int defaultColor = HubColors.getHubBottomToolbarColor(mActivity, HubColorScheme.DEFAULT);
        int incognitoColor =
                HubColors.getHubBottomToolbarColor(mActivity, HubColorScheme.INCOGNITO);

        mPropertyModel.set(COLOR_SCHEME, HubColorScheme.INCOGNITO);
        assertEquals(
                incognitoColor, ((ColorDrawable) mBottomToolbarView.getBackground()).getColor());

        mPropertyModel.set(COLOR_SCHEME, HubColorScheme.DEFAULT);
        assertEquals(defaultColor, ((ColorDrawable) mBottomToolbarView.getBackground()).getColor());
    }

    @Test
    @SmallTest
    public void testColorMixer() {
        verify(mHubColorMixer).registerBlend(mBlendCaptor.capture());
        HubViewColorBlend blend = mBlendCaptor.getValue();

        int defaultColor = HubColors.getHubBottomToolbarColor(mActivity, HubColorScheme.DEFAULT);
        int incognitoColor =
                HubColors.getHubBottomToolbarColor(mActivity, HubColorScheme.INCOGNITO);

        // Blend updates apply to the view.
        blend.updateProgress(HubColorScheme.DEFAULT, HubColorScheme.INCOGNITO, 1.0f);
        assertEquals(
                incognitoColor, ((ColorDrawable) mBottomToolbarView.getBackground()).getColor());

        blend.updateProgress(HubColorScheme.INCOGNITO, HubColorScheme.DEFAULT, 1.0f);
        assertEquals(defaultColor, ((ColorDrawable) mBottomToolbarView.getBackground()).getColor());

        // When COLOR_MIXER is cleared, blend is unregistered.
        mPropertyModel.set(COLOR_MIXER, null);
        verify(mHubColorMixer).unregisterBlend(blend);

        // Re-attaching COLOR_MIXER re-registers blend
        mPropertyModel.set(COLOR_MIXER, mHubColorMixer);
        verify(mHubColorMixer, times(2)).registerBlend(blend);
    }
}
