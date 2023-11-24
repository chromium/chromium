// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import android.content.ComponentName;

import androidx.annotation.Nullable;

import org.chromium.chrome.browser.tab.Tab;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.WebContents;

/** Interface for handling tab related async operations over multiple activities. */
public interface AsyncTabParams {
    /**
     * @return The {@link LoadUrlParams} associated with the initial URL to load.
     */
    LoadUrlParams getLoadUrlParams();

    /**
     * @return The request ID (tab ID) for this {@link AsyncTabParams}.
     */
    @Nullable
    Integer getRequestId();

    /**
     * @return The {@link WebContents} associated with this {@link AsyncTabParams}.
     */
    @Nullable
    WebContents getWebContents();

    /**
     * @return The {@link ComponentName} that should handle this Intent.
     */
    @Nullable
    ComponentName getComponentName();

    /**
     * @return The tab that would be reparenting through this {@link AsyncTabParams}.
     */
    @Nullable
    Tab getTabToReparent();

    /** Destroy any internal fields if it is necessary. */
    void destroy();
}
