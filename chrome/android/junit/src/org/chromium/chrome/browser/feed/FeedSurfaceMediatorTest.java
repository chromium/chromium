// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;

/**
 * Tests for {@link FeedSurfaceMediator}.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class FeedSurfaceMediatorTest {
    @Test
    public void testSerializeScrollState() {
        FeedSurfaceMediator.ScrollState state = new FeedSurfaceMediator.ScrollState();
        state.tabId = 5;
        state.position = 2;
        state.lastPosition = 4;
        state.offset = 50;

        FeedSurfaceMediator.ScrollState deserializedState =
                FeedSurfaceMediator.ScrollState.fromJson(state.toJson());

        Assert.assertEquals(2, deserializedState.position);
        Assert.assertEquals(4, deserializedState.lastPosition);
        Assert.assertEquals(50, deserializedState.offset);
        Assert.assertEquals(5, deserializedState.tabId);
        Assert.assertEquals(state.toJson(), deserializedState.toJson());
    }

    @Test
    public void testScrollStateFromInvalidJson() {
        Assert.assertEquals(null, FeedSurfaceMediator.ScrollState.fromJson("{{=xcg"));
    }
}
