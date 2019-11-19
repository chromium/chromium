// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import static org.mockito.Mockito.any;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.reset;
import static org.mockito.Mockito.verify;

import android.app.Activity;
import android.content.Context;
import android.graphics.Rect;
import android.support.test.filters.SmallTest;
import android.view.WindowInsets;
import android.widget.LinearLayout;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;

import org.chromium.testing.local.LocalRobolectricTestRunner;

/**
 * Tests for {@link InsetObserverView} class.
 */
@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class InsetObserverViewTest {
    /** The rect values if the display cutout is present. */
    private static final Rect DISPLAY_CUTOUT_RECT = new Rect(1, 1, 1, 1);

    /** The rect values if there is no cutout. */
    private static final Rect NO_CUTOUT_RECT = new Rect(0, 0, 0, 0);

    /** Mock Android P+ Display Cutout class. */
    private static class DisplayCutout {
        public int getSafeInsetLeft() {
            return 1;
        }

        public int getSafeInsetTop() {
            return 1;
        }

        public int getSafeInsetRight() {
            return 1;
        }

        public int getSafeInsetBottom() {
            return 1;
        }
    }

    /** This is a {@InsetObserverView} that will use a fake display cutout. */
    private static class TestInsetObserverView extends InsetObserverView.InsetObserverViewApi28 {
        private Object mDisplayCutout;

        TestInsetObserverView(Context context, Object displayCutout) {
            super(context);
            mDisplayCutout = displayCutout;
        }

        @Override
        protected Object extractDisplayCutout(WindowInsets insets) {
            return mDisplayCutout;
        }

        public void setDisplayCutout(DisplayCutout displayCutout) {
            mDisplayCutout = displayCutout;
        }
    }

    @Mock
    private InsetObserverView.WindowInsetObserver mObserver;

    @Mock
    private WindowInsets mInsets;

    private Activity mActivity;

    private TestInsetObserverView mView;

    private LinearLayout mContentView;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);

        mActivity = Robolectric.buildActivity(Activity.class).setup().get();
        mContentView = new LinearLayout(mActivity);
        mActivity.setContentView(mContentView);
    }

    /** Test that applying new insets does not notify the observer.. */
    @Test
    @SmallTest
    public void applyInsets() {
        mView = new TestInsetObserverView(mActivity, null);
        mView.addObserver(mObserver);

        mView.onApplyWindowInsets(mInsets);
        verify(mObserver, never()).onSafeAreaChanged(any());
    }

    /** Test that applying new insets with a cutout notifies the observer. */
    @Test
    @SmallTest
    public void applyInsets_WithCutout() {
        mView = new TestInsetObserverView(mActivity, new DisplayCutout());
        mView.addObserver(mObserver);

        mView.onApplyWindowInsets(mInsets);
        verify(mObserver).onSafeAreaChanged(DISPLAY_CUTOUT_RECT);
    }

    /** Test applying new insets with a cutout and then remove the cutout. */
    @Test
    @SmallTest
    public void applyInsets_WithCutout_WithoutCutout() {
        mView = new TestInsetObserverView(mActivity, new DisplayCutout());
        mView.addObserver(mObserver);

        mView.onApplyWindowInsets(mInsets);
        verify(mObserver).onSafeAreaChanged(DISPLAY_CUTOUT_RECT);

        reset(mObserver);
        mView.setDisplayCutout(null);
        mView.onApplyWindowInsets(mInsets);
        verify(mObserver).onSafeAreaChanged(NO_CUTOUT_RECT);
    }

    /** Test that applying new insets with a cutout but no observer is a no-op. */
    @Test
    @SmallTest
    public void applyInsets_WithCutout_NoListener() {
        mView = new TestInsetObserverView(mActivity, new DisplayCutout());
        mView.onApplyWindowInsets(mInsets);
    }

    /** Test that applying new insets with no observer is a no-op. */
    @Test
    @SmallTest
    public void applyInsets_NoListener() {
        mView = new TestInsetObserverView(mActivity, null);
        mView.onApplyWindowInsets(mInsets);
    }
}
