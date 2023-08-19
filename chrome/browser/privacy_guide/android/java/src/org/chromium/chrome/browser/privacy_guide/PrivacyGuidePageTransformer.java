// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.privacy_guide;

import android.view.View;

import androidx.viewpager2.widget.ViewPager2;

public class PrivacyGuidePageTransformer implements ViewPager2.PageTransformer {
    private static final float TRANSLATION_COEFFICIENT = 0.85f;

    @Override
    public void transformPage(View view, float position) {
        if (position <= -1f || position >= 1f) {
            view.setAlpha(0f);
            view.setTranslationX(0f);
            view.setVisibility(View.GONE);
        } else {
            view.setAlpha(1f - Math.abs(position));
            view.setTranslationX(-position * (view.getWidth() * TRANSLATION_COEFFICIENT));
            view.setVisibility(View.VISIBLE);
        }
    }
}
