// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.lifecycle;

import org.chromium.build.annotations.NullMarked;

/** Simple interface that provides {@link ActivityLifecycleDispatcher}. */
@NullMarked
public interface ActivityLifecycleDispatcherProvider {

    /** Return the {@link ActivityLifecycleDispatcher} associated with this provider. */
    ActivityLifecycleDispatcher getLifecycleDispatcher();
}
