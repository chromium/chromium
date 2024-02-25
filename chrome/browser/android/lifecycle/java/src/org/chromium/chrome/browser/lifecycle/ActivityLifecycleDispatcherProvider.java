// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.lifecycle;

/** Simple interface that provides {@link ActivityLifecycleDispatcher}. */
public interface ActivityLifecycleDispatcherProvider {

    /** Return the {@link ActivityLifecycleDispatcher} associated with this provider. */
    ActivityLifecycleDispatcher getLifecycleDispatcher();
}
