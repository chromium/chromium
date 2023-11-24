// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments.ui;

import android.graphics.drawable.Drawable;
import android.util.IntProperty;

/** Holds different {@link android.util.Property} types that can be used with ObjectAnimators. */
class AnimatorProperties {
    static final IntProperty<Drawable> DRAWABLE_ALPHA_PROPERTY =
            new IntProperty<Drawable>("alpha") {
                @Override
                public Integer get(Drawable d) {
                    return d.getAlpha();
                }

                @Override
                public void setValue(Drawable d, int alpha) {
                    d.setAlpha(alpha);
                }
            };
}
