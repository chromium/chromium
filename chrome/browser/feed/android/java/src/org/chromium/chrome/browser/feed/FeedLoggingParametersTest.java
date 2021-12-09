// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import androidx.test.filters.SmallTest;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.components.feed.proto.FeedUiProto;

/** Test for FeedLoggingParameters. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public final class FeedLoggingParametersTest {
    @Test
    @SmallTest
    public void testEquality() {
        FeedLoggingParameters allNull = new FeedLoggingParameters(null, null, false, false);

        assertFalse(allNull.loggingParametersEquals(null));

        // Null and empty string are considered equivalent.
        assertTrue(
                new FeedLoggingParameters("", "", false, false).loggingParametersEquals(allNull));
        assertTrue(
                allNull.loggingParametersEquals(new FeedLoggingParameters("", "", false, false)));
        assertFalse(new FeedLoggingParameters(null, null, false, true)
                            .loggingParametersEquals(allNull));
        assertFalse(new FeedLoggingParameters(null, null, true, false)
                            .loggingParametersEquals(allNull));
        assertFalse(new FeedLoggingParameters(null, "a", false, false)
                            .loggingParametersEquals(allNull));
        assertFalse(new FeedLoggingParameters("a", null, false, false)
                            .loggingParametersEquals(allNull));

        assertTrue(
                new FeedLoggingParameters("a", "b", true, true)
                        .loggingParametersEquals(new FeedLoggingParameters("a", "b", true, true)));
        assertFalse(
                new FeedLoggingParameters("a", "b", true, true)
                        .loggingParametersEquals(new FeedLoggingParameters("x", "b", true, true)));
        assertFalse(
                new FeedLoggingParameters("a", "b", true, true)
                        .loggingParametersEquals(new FeedLoggingParameters("a", "x", true, true)));
        assertFalse(
                new FeedLoggingParameters("a", "b", true, true)
                        .loggingParametersEquals(new FeedLoggingParameters("a", "b", false, true)));
        assertFalse(
                new FeedLoggingParameters("a", "b", true, true)
                        .loggingParametersEquals(new FeedLoggingParameters("a", "b", true, false)));
    }

    @Test
    @SmallTest
    public void testFromProto() {
        FeedUiProto.LoggingParameters proto = FeedUiProto.LoggingParameters.newBuilder()
                                                      .setEmail("user@foo.com")
                                                      .setClientInstanceId("cid")
                                                      .setLoggingEnabled(true)
                                                      .setViewActionsEnabled(false)
                                                      .build();
        assertTrue(new FeedLoggingParameters(proto).loggingParametersEquals(
                new FeedLoggingParameters("cid", "user@foo.com", true, false)));
    }
}
