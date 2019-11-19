// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browserservices;

import android.content.Intent;
import android.os.Bundle;

import androidx.annotation.Nullable;

import org.chromium.chrome.browser.tabmodel.TabModelSelector;

/**
 * Interface for controlling Activity's tabs.
 */
public interface BrowserServicesActivityTabController {
    /**
     * Detaches the tab and starts reparenting into the browser using given {@param intent} and
     * {@param startActivityOptions}.
     */
    void detachAndStartReparenting(
            Intent intent, Bundle startActivityOptions, Runnable finishCallback);

    /** Closes the current tab. */
    void closeTab();

    /** Closes the tab and deletes related metadata. */
    void closeAndForgetTab();

    /** Save the current state of the tab. */
    void saveState();

    /** Returns {@link TabModelSelector}. Should be called after postInflationStartup. */
    public @Nullable TabModelSelector getTabModelSelector();
}
