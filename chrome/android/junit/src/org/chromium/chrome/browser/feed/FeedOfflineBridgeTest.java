// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;

import android.support.test.filters.SmallTest;

import com.google.android.libraries.feed.api.client.knowncontent.ContentRemoval;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;

import java.util.ArrayList;
import java.util.List;

/** Unit tests for {@link FeedOfflineBridge}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class FeedOfflineBridgeTest {
    @Test
    @SmallTest
    public void testUserDrivenRemovalsOnlyMultiple() {
        List<ContentRemoval> removals = new ArrayList<>();
        removals.add(new ContentRemoval("a", true));
        removals.add(new ContentRemoval("b", false));
        removals.add(new ContentRemoval("c", true));
        removals.add(new ContentRemoval("d", false));
        List<String> filtered = FeedOfflineBridge.takeUserDriveRemovalsOnly(removals);
        assertEquals(2, filtered.size());
        assertTrue(filtered.contains("a"));
        assertTrue(filtered.contains("c"));
    }

    @Test
    @SmallTest
    public void testUserDrivenRemovalsOnlyEmpty() {
        List<String> filtered = FeedOfflineBridge.takeUserDriveRemovalsOnly(new ArrayList<>());
        assertEquals(0, filtered.size());
    }
}
