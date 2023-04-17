// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.shared.ui;

import static com.google.common.truth.Truth.assertThat;

import android.app.Activity;
import android.view.View;
import android.widget.FrameLayout;

import androidx.swiperefreshlayout.widget.CircularProgressDrawable;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;
import org.robolectric.annotation.LooperMode;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.R;

/** Tests for {@link MaterialSpinnerView}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@LooperMode(LooperMode.Mode.PAUSED)
public class MaterialSpinnerViewTest {
    private FrameLayout mLayout;
    private MaterialSpinnerView mMaterialSpinnerView;
    private CircularProgressDrawable mAnimationDrawable;

    @Before
    public void setUp() {
        Activity activity = Robolectric.setupActivity(Activity.class);
        // First set the app theme, then apply the feed theme overlay.
        activity.setTheme(R.style.Theme_BrowserUI);
        activity.setTheme(R.style.ThemeOverlay_Feed_Light);

        // Attach the spinner inside a layout, so we can hide either the spinner
        // or the parent view (ie. the layout) in the tests. Note that we
        // require the looper to stay paused (LooperMode.Mode.PAUSED) for the
        // duration of the tests. Otherwise, Robolectric will run through the
        // animation and stop it before the tests get run. Because
        // Robolectric.setupActivity() will run the looper until idle, we call
        // setContentView() only after launching the activity above.
        mMaterialSpinnerView = new MaterialSpinnerView(activity);
        mAnimationDrawable = (CircularProgressDrawable) mMaterialSpinnerView.getDrawable();

        mLayout = new FrameLayout(activity);
        mLayout.addView(mMaterialSpinnerView);
        activity.setContentView(mLayout);
    }

    @Test
    public void testInit_isVisible_spinnerStarted() {
        assertThat(mMaterialSpinnerView.getVisibility()).isEqualTo(View.VISIBLE);
        assertThat(mMaterialSpinnerView.isShown()).isTrue();

        assertThat(mAnimationDrawable.isRunning()).isTrue();
    }

    @Test
    public void testSetVisibility_gone_stopsSpinner() {
        mMaterialSpinnerView.setVisibility(View.GONE);

        assertThat(mAnimationDrawable.isRunning()).isFalse();
    }

    @Test
    public void testSetVisibility_invisible_stopsSpinner() {
        mMaterialSpinnerView.setVisibility(View.INVISIBLE);

        assertThat(mAnimationDrawable.isRunning()).isFalse();
    }

    @Test
    public void testSetVisibility_toTrue_startsSpinner() {
        mMaterialSpinnerView.setVisibility(View.GONE);
        mMaterialSpinnerView.setVisibility(View.VISIBLE);

        assertThat(mAnimationDrawable.isRunning()).isTrue();
    }

    @Test
    public void testContainerSetVisibility_gone_stopsSpinner() {
        mLayout.setVisibility(View.GONE);

        assertThat(mAnimationDrawable.isRunning()).isFalse();
    }

    @Test
    public void testContainerSetVisibility_invisible_stopsSpinner() {
        mLayout.setVisibility(View.INVISIBLE);

        assertThat(mAnimationDrawable.isRunning()).isFalse();
    }

    @Test
    public void testContainerSetVisibility_toTrue_startsSpinner() {
        mLayout.setVisibility(View.GONE);
        mLayout.setVisibility(View.VISIBLE);

        assertThat(mAnimationDrawable.isRunning()).isTrue();
    }

    @Test
    public void testDetachFromWindow_stopsSpinner() {
        mLayout.removeView(mMaterialSpinnerView);

        assertThat(mAnimationDrawable.isRunning()).isFalse();
    }

    @Test
    public void testAttachToWindow_startsSpinner() {
        mLayout.removeView(mMaterialSpinnerView);
        mLayout.addView(mMaterialSpinnerView);

        assertThat(mAnimationDrawable.isRunning()).isTrue();
    }
}
