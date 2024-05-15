// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.test.util;

import android.view.View;

import androidx.test.espresso.FailureHandler;

import org.hamcrest.Matcher;

/**
 * Espresso FailureHandler that does not dump View hierarchy or take a screenshot.
 *
 * <p>This reduces time to throw an Exception by an order of magnitude. Useful when we expect an
 * Exception to be thrown in a ViewInteraction.
 */
public class RawFailureHandler implements FailureHandler {

    private static RawFailureHandler sInstance = new RawFailureHandler();

    public static RawFailureHandler getInstance() {
        return sInstance;
    }

    @Override
    public void handle(Throwable throwable, Matcher<View> matcher) {
        // Cannot just throw the Throwable because the FailureHandle#handle() declaration
        // does not specify it throws any Exception, so we need to throw unchecked exceptions and
        // wrap checked exceptions.

        if (throwable instanceof RuntimeException) {
            throw (RuntimeException) throwable;
        }

        if (throwable instanceof Error) {
            throw (Error) throwable;
        }

        throw new RuntimeException(throwable);
    }
}
