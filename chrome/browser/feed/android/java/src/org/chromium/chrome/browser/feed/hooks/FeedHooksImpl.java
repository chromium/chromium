// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.hooks;

/**
 * A stub FeedHooks implementation. Replace at build time on internal builds.
 */
public class FeedHooksImpl implements FeedHooks {
    public static FeedHooks getInstance() {
        return new FeedHooksImpl() {};
    }
}
