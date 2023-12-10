// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui;

import android.view.View;
import android.view.ViewTreeObserver.OnPreDrawListener;

import org.chromium.base.supplier.Supplier;

/** Class that blocks a {@link View} from drawing until a condition is true. */
public class ViewDrawBlocker {
    /**
     * Blocks |view|'s draw until |viewReadySupplier| is true.
     * @param view {@link View} that will be blocked from drawing.
     * @param viewReadySupplier {@link Supplier} to denote when the view is ready to draw.
     */
    public static void blockViewDrawUntilReady(View view, Supplier<Boolean> viewReadySupplier) {
        view.getViewTreeObserver()
                .addOnPreDrawListener(
                        new OnPreDrawListener() {
                            @Override
                            public boolean onPreDraw() {
                                if (!viewReadySupplier.get()) return false;

                                view.getViewTreeObserver().removeOnPreDrawListener(this);
                                return true;
                            }
                        });
    }
}
