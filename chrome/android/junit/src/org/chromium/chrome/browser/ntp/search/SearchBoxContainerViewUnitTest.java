// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp.search;

import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.verify;

import android.content.Context;
import android.view.ContextThemeWrapper;
import android.view.InputDevice;
import android.view.LayoutInflater;
import android.view.MotionEvent;

import androidx.test.core.app.ApplicationProvider;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.ntp.R;
import org.chromium.chrome.browser.omnibox.GlifStrokeDrawable;
import org.chromium.components.browser_ui.util.motion.MotionEventTestUtils;

/** Unit tests for {@link SearchBoxContainerView}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class SearchBoxContainerViewUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    private Context mContext;
    private SearchBoxContainerView mSearchBoxLayout;
    private GlifStrokeDrawable mGlifStrokeDrawable;

    @Before
    public void setup() {
        mContext =
                new ContextThemeWrapper(
                        ApplicationProvider.getApplicationContext(),
                        R.style.Theme_BrowserUI_DayNight);
        mSearchBoxLayout =
                (SearchBoxContainerView)
                        LayoutInflater.from(mContext)
                                .inflate(R.layout.fake_search_box_layout, null);
        mGlifStrokeDrawable = spy(mSearchBoxLayout.mGlifStrokeDrawable);
        mSearchBoxLayout.mGlifStrokeDrawable = mGlifStrokeDrawable;
    }

    @Test
    public void testGlifStrokeDrawable_HoverEnters() {
        mSearchBoxLayout.mAiChip.setVisibility(android.view.View.VISIBLE);

        MotionEvent hoverEnterEvent =
                MotionEventTestUtils.createMotionEvent(
                        /* downTime= */ 1,
                        /* eventTime= */ 1,
                        /* action= */ MotionEvent.ACTION_HOVER_ENTER,
                        /* x= */ 0,
                        /* y= */ 0,
                        /* source= */ InputDevice.SOURCE_CLASS_POINTER,
                        /* toolType= */ MotionEvent.TOOL_TYPE_MOUSE);
        mSearchBoxLayout.mAiChip.dispatchGenericMotionEvent(hoverEnterEvent);

        verify(mGlifStrokeDrawable).start();
    }
}
