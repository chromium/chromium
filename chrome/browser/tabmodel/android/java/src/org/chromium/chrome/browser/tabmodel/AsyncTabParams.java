// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import android.content.ComponentName;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.WebContents;

/** Interface for handling tab related async operations over multiple activities. */
@NullMarked
public interface AsyncTabParams {
    /** Returns the {@link LoadUrlParams} associated with the initial URL to load. */
    @Nullable LoadUrlParams getLoadUrlParams();

    /** Returns the request ID (tab ID) for this {@link AsyncTabParams}. */
    @Nullable Integer getRequestId();

    /** Returns the {@link WebContents} associated with this {@link AsyncTabParams}. */
    @Nullable WebContents getWebContents();

    /** Returns the {@link ComponentName} that should handle this Intent. */
    @Nullable ComponentName getComponentName();

    /** Returns the tab that would be reparenting through this {@link AsyncTabParams}. */
    @Nullable Tab getTabToReparent();

    /**
     * Returns whether the tab to reparent should be reloaded once attached during activity
     * recreation.
     */
    boolean shouldReload();

    /** Returns the callback to run once tab reparenting has finished, if any. */
    @Nullable Runnable getFinalizeCallback();

    /** Detaches the tab to reparent from its source {@link TabModelSelector}, if still pending. */
    void detachFromOriginalModel();

    /** Destroy any internal fields if it is necessary. */
    void destroy();
}
