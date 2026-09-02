// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.safety_promo;

import android.view.View.OnClickListener;

import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel.ReadableObjectPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableIntPropertyKey;

/** Properties defined for Safety Promo Carousel. */
@NullMarked
public class SafetyPromoCarouselProperties {
    public static final WritableIntPropertyKey TITLE_RES_ID = new WritableIntPropertyKey();

    public static final WritableIntPropertyKey SUBTITLE_RES_ID = new WritableIntPropertyKey();

    public static final ReadableObjectPropertyKey<OnClickListener> ON_CONTINUE_CLICKED =
            new ReadableObjectPropertyKey<>();

    public static final PropertyKey[] ALL_KEYS =
            new PropertyKey[] {TITLE_RES_ID, SUBTITLE_RES_ID, ON_CONTINUE_CLICKED};
}
