// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui;

import android.view.View;
import android.view.ViewTreeObserver;
import android.view.ViewTreeObserver.OnPreDrawListener;

import org.chromium.build.annotations.NullMarked;

import java.util.function.Supplier;

/** Class that blocks a {@link View} from drawing until a condition is true. */
@NullMarked
public class ViewDrawBlocker {
    /**
     * Blocks |view|'s draw until |viewReadySupplier| is true.
     *
     * @param view {@link View} that will be blocked from drawing.
     * @param viewReadySupplier {@link Supplier} to denote when the view is ready to draw.
     * @return A {@link Runnable} that stops blocking by removing the pre-draw listener, whether or
     *     not |viewReadySupplier| is true. Running it after the listener was removed is a no-op.
     */
    public static Runnable blockViewDrawUntilReady(View view, Supplier<Boolean> viewReadySupplier) {
        OnPreDrawListener listener =
                new OnPreDrawListener() {
                    @Override
                    public boolean onPreDraw() {
                        if (!viewReadySupplier.get()) return false;

                        view.getViewTreeObserver().removeOnPreDrawListener(this);
                        return true;
                    }
                };
        ViewTreeObserver registeredObserver = view.getViewTreeObserver();
        registeredObserver.addOnPreDrawListener(listener);
        return () -> {
            // A view's observer changes when the view is attached to a window, so the listener
            // may be on the observer it was added to or on the view's current one.
            ViewTreeObserver currentObserver = view.getViewTreeObserver();
            currentObserver.removeOnPreDrawListener(listener);
            if (registeredObserver != currentObserver && registeredObserver.isAlive()) {
                registeredObserver.removeOnPreDrawListener(listener);
            }
        };
    }
}
