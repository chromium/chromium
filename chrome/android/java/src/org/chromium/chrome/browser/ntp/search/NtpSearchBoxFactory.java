// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp.search;

import android.content.Context;
import android.view.ViewGroup;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.ui.base.WindowAndroid;

/** Factory for creating {@link NtpSearchBox} instances. */
@NullMarked
public class NtpSearchBoxFactory {
    public static NtpSearchBox createSearchBox(
            Context context,
            ViewGroup parent,
            boolean isTablet,
            ActivityLifecycleDispatcher activityLifecycleDispatcher,
            boolean isIncognito,
            WindowAndroid windowAndroid) {
        return new SearchBoxCoordinator(
                context, parent, isTablet, activityLifecycleDispatcher, isIncognito, windowAndroid);
    }
}
