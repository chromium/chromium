// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.metrics.test;

import org.robolectric.annotation.Implementation;
import org.robolectric.annotation.Implements;

import org.chromium.base.metrics.RecordUserAction;
import org.chromium.base.metrics.RecordUserAction.UserActionCallback;

import java.util.ArrayList;
import java.util.List;
import java.util.Queue;
import java.util.concurrent.ArrayBlockingQueue;

/**
 * Implementation of {@link RecordUserAction} which does not rely on native and still enables
 * testing of user action counts. {@link ShadowRecordUserAction#reset} should be called in the
 * {@link @Before} method of test cases using this class.
 *
 * <p>Note that this class uses {@link ArrayBlockingQueue} to store the samples, and it has a
 * capacity of 100 - an arbitrary number assuming most unit test does not record samples more than
 * that.</p>
 *
 * TODO(https://crbug.com/1211884): Remove this class once UmaRecorderHolder setup is available for
 * junit tests.
 */
@Implements(RecordUserAction.class)
public class ShadowRecordUserAction {
    private static final Queue<String> sSamples = new ArrayBlockingQueue<>(100);

    /**
     * Get all the user action samples tracked by this class, organized in the order it is
     * triggered.
     */
    public static List<String> getSamples() {
        return new ArrayList<>(sSamples);
    }

    /** Clear all the samples tracked by the shadow. */
    public static void reset() {
        sSamples.clear();
    }

    @Implementation
    protected static void record(final String action) {
        sSamples.add(action);
    }

    @Implementation
    protected static void setActionCallbackForTesting(UserActionCallback callback) {
        assert false : "Should not be used in Robolectric tests.";
    }

    @Implementation
    protected static void removeActionCallbackForTesting() {
        assert false : "Should not be used in Robolectric tests.";
    }
}
