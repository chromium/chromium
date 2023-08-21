// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed;

/**
 * Provides instances needed in response to Native calls.
 *
 * Note that certain instances are sometimes needed in response to a Native call, we
 * use this through reflection rather than simply injecting an instance.
 */
public interface FeedServiceDependencyProviderFactory {
    /** Constructs and returns a FeedServiceUtil instance. */
    FeedServiceUtil createFeedServiceUtil();
}
