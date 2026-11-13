// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.keyboard_accessory.bar_component;

import static org.junit.Assert.assertFalse;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.content.Context;
import android.graphics.Rect;
import android.view.View;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.build.annotations.NullMarked;
import org.chromium.components.feature_engagement.FeatureConstants;
import org.chromium.components.feature_engagement.FeatureProperties;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.components.feature_engagement.UiFormat;
import org.chromium.ui.widget.RectProvider;

/** Unit tests for {@link KeyboardAccessoryIphUtils}. */
@NullMarked
@RunWith(BaseRobolectricTestRunner.class)
public class KeyboardAccessoryIphUtilsTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private Tracker mTracker;
    @Mock private Runnable mDismissCallback;

    private Context mContext;
    private View mRootView;
    private final RectProvider mRectProvider = new RectProvider(new Rect(0, 0, 10, 10));

    @Before
    public void setUp() {
        mContext = ContextUtils.getApplicationContext();
        mRootView = new View(mContext);
        when(mTracker.isInitialized()).thenReturn(true);
        when(mTracker.shouldTriggerHelpUi(any(), any())).thenReturn(false);
    }

    @Test
    public void testShowHelpBubble() {
        assertFalse(
                KeyboardAccessoryIphUtils.showHelpBubble(
                        mTracker,
                        FeatureConstants.KEYBOARD_ACCESSORY_EXTERNAL_ACCOUNT_PROFILE_FEATURE,
                        mRectProvider,
                        mContext,
                        mRootView,
                        mDismissCallback));

        verify(mTracker)
                .shouldTriggerHelpUi(
                        eq(FeatureConstants.KEYBOARD_ACCESSORY_EXTERNAL_ACCOUNT_PROFILE_FEATURE),
                        eq(new FeatureProperties(UiFormat.BLUE_BUBBLE)));
    }
}
