// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.shared.ui;

import static com.google.common.truth.Truth.assertThat;

import android.app.Activity;
import android.view.View;

import androidx.swiperefreshlayout.widget.CircularProgressDrawable;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;

import org.chromium.chrome.R;
import org.chromium.testing.local.LocalRobolectricTestRunner;

/** Tests for {@link MaterialSpinnerView}. */
@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class MaterialSpinnerViewTest {
    private MaterialSpinnerView mMaterialSpinnerView;

    @Before
    public void setUp() {
        Activity context = Robolectric.buildActivity(Activity.class).get();
        context.setTheme(R.style.Light);
        mMaterialSpinnerView = new MaterialSpinnerView(context);
    }

    @Test
    public void testInit_isVisible_spinnerStarted() {
        assertThat(mMaterialSpinnerView.getVisibility()).isEqualTo(View.VISIBLE);

        assertThat(((CircularProgressDrawable) mMaterialSpinnerView.getDrawable()).isRunning())
                .isTrue();
    }

    @Test
    public void testSetVisibility_gone_stopsSpinner() {
        mMaterialSpinnerView.setVisibility(View.GONE);

        assertThat(((CircularProgressDrawable) mMaterialSpinnerView.getDrawable()).isRunning())
                .isFalse();
    }

    @Test
    public void testSetVisibility_invisible_stopsSpinner() {
        mMaterialSpinnerView.setVisibility(View.INVISIBLE);

        assertThat(((CircularProgressDrawable) mMaterialSpinnerView.getDrawable()).isRunning())
                .isFalse();
    }

    @Test
    public void testSetVisibility_toTrue_startsSpinner() {
        mMaterialSpinnerView.setVisibility(View.GONE);
        mMaterialSpinnerView.setVisibility(View.VISIBLE);

        assertThat(((CircularProgressDrawable) mMaterialSpinnerView.getDrawable()).isRunning())
                .isTrue();
    }
}
