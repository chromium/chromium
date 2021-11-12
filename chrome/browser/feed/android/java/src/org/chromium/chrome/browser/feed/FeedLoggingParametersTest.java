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
        FeedLoggingParameters allNull = new FeedLoggingParameters(null, null, null);

        assertFalse(allNull.loggingParametersEquals(null));

        // Null and empty string are considered equivalent.
        assertTrue(new FeedLoggingParameters("", "", "").loggingParametersEquals(allNull));
        assertTrue(allNull.loggingParametersEquals(new FeedLoggingParameters("", "", "")));
        assertFalse(new FeedLoggingParameters(null, null, "a").loggingParametersEquals(allNull));
        assertFalse(new FeedLoggingParameters(null, "a", null).loggingParametersEquals(allNull));
        assertFalse(new FeedLoggingParameters("a", null, null).loggingParametersEquals(allNull));

        assertTrue(new FeedLoggingParameters("a", "b", "c")
                           .loggingParametersEquals(new FeedLoggingParameters("a", "b", "c")));
        assertFalse(new FeedLoggingParameters("a", "b", "c")
                            .loggingParametersEquals(new FeedLoggingParameters("x", "b", "c")));
        assertFalse(new FeedLoggingParameters("a", "b", "c")
                            .loggingParametersEquals(new FeedLoggingParameters("a", "x", "c")));
        assertFalse(new FeedLoggingParameters("a", "b", "c")
                            .loggingParametersEquals(new FeedLoggingParameters("a", "x", "x")));
    }

    @Test
    @SmallTest
    public void testFromProto() {
        FeedUiProto.LoggingParameters proto = FeedUiProto.LoggingParameters.newBuilder()
                                                      .setEmail("user@foo.com")
                                                      .setClientInstanceId("cid")
                                                      .setSessionId("session")
                                                      .build();
        assertTrue(new FeedLoggingParameters(proto).loggingParametersEquals(
                new FeedLoggingParameters("cid", "user@foo.com", "session")));
    }
}
