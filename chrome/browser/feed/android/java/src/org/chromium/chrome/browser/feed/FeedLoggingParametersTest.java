// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed;

import static org.junit.Assert.assertArrayEquals;
import static org.junit.Assert.assertEquals;

import androidx.test.filters.SmallTest;

import com.google.protobuf.ByteString;

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
    public void testFields() {
        FeedLoggingParameters params =
                new FeedLoggingParameters(
                        /* clientInstanceId= */ "instance-id",
                        /* accountName= */ "account",
                        /* loggingEnabled= */ false,
                        /* viewActionsEnabled= */ true,
                        /* rootEventId= */ new byte[] {5});

        assertEquals(params.clientInstanceId(), "instance-id");
        assertEquals(params.accountName(), "account");
        assertEquals(params.loggingEnabled(), false);
        assertEquals(params.viewActionsEnabled(), true);
        assertArrayEquals(params.rootEventId(), new byte[] {5});
    }

    @Test
    @SmallTest
    public void testFromProto() {
        FeedUiProto.LoggingParameters proto =
                FeedUiProto.LoggingParameters.newBuilder()
                        .setEmail("account")
                        .setClientInstanceId("instance-id")
                        .setLoggingEnabled(false)
                        .setViewActionsEnabled(true)
                        .setRootEventId(ByteString.copyFrom(new byte[] {5}))
                        .build();
        FeedLoggingParameters parsed = new FeedLoggingParameters(proto);
        assertEquals(parsed.clientInstanceId(), "instance-id");
        assertEquals(parsed.accountName(), "account");
        assertEquals(parsed.loggingEnabled(), false);
        assertEquals(parsed.viewActionsEnabled(), true);
        assertArrayEquals(parsed.rootEventId(), new byte[] {5});
        assertEquals(proto, FeedLoggingParameters.convertToProto(parsed));
    }

    @Test
    @SmallTest
    public void testFromProto_noRootEventId() {
        FeedUiProto.LoggingParameters proto =
                FeedUiProto.LoggingParameters.newBuilder()
                        .setEmail("user@foo.com")
                        .setClientInstanceId("cid")
                        .setLoggingEnabled(true)
                        .setViewActionsEnabled(false)
                        .build();
        FeedLoggingParameters parsed = new FeedLoggingParameters(proto);
        assertArrayEquals(parsed.rootEventId(), new byte[] {});
        assertEquals(proto, FeedLoggingParameters.convertToProto(parsed));
    }
}
