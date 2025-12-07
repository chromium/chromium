// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs.features.minimizedcustomtab;

import org.chromium.build.annotations.NullMarked;

/** Delegate for Minimized Custom Tabs feature engagement. */
@NullMarked
interface MinimizedCustomTabFeatureEngagementDelegate {
    /** Notify the feature engagement system that the user engaged with the minimize button. */
    void notifyUserEngaged();
}
