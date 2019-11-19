// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.directactions;

import android.annotation.TargetApi;

import java.util.ArrayList;
import java.util.List;
import java.util.function.Consumer;

/**
 * Implementation of {@link DirectActionReporter} suitable for tests.
 *
 * <p>While Direct Actions are only available since Android Q, this test avoids using any type that
 * is specific to Android Q, so can run on older versions of the API. TODO(crbug.com/973781): Once
 * Chromium is built against Android Q SDK, have the test use {@code android.app.DirectAction}
 * directly.
 */
@TargetApi(24) // for java.util.function.Consumer.
public class FakeDirectActionReporter implements DirectActionReporter {
    private final Consumer<List<String>> mCallback;
    private final List<String> mDirectActions = new ArrayList<>();

    public FakeDirectActionReporter() {
        this(null);
        // When using this constructor, actions can only be accessed using getDirectActions()
    }

    public FakeDirectActionReporter(Consumer<List<String>> callback) {
        mCallback = callback;
    }

    public List<String> getDirectActions() {
        return mDirectActions;
    }

    @Override
    public void report() {
        mCallback.accept(mDirectActions);
    }

    @Override
    public DirectActionReporter.Definition addDirectAction(String actionId) {
        mDirectActions.add(actionId);
        return null;
    }
}
