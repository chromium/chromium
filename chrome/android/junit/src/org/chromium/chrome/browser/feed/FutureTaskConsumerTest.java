// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed;

import static org.junit.Assert.assertEquals;

import android.support.test.filters.SmallTest;

import com.google.android.libraries.feed.common.functional.Consumer;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.Callback;
import org.chromium.base.test.BaseRobolectricTestRunner;

/** Unit tests for {@link FutureTaskConsumer}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class FutureTaskConsumerTest {
    @Test
    @SmallTest
    public void testConsume() {
        Integer expected = 42;
        Integer failure = -1;

        Callback<Consumer<Integer>> callback = new Callback<Consumer<Integer>>() {
            @Override
            public void onResult(Consumer<Integer> result) {
                result.accept(expected);
            }
        };

        Integer actual = FutureTaskConsumer.consume("", callback, failure);

        assertEquals(expected, actual);
    }
}
