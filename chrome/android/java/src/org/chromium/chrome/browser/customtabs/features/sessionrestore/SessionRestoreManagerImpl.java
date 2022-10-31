// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs.features.sessionrestore;

import androidx.annotation.Nullable;

import org.chromium.chrome.browser.tab.Tab;

/**
 * Default implementation for SessionRestoreManager.
 */
public class SessionRestoreManagerImpl implements SessionRestoreManager {
    @Override
    public boolean store(Tab tabToRestore) {
        return false;
    }

    @Nullable
    @Override
    public Tab restoreTab() {
        return null;
    }

    @Override
    public boolean canRestoreTab() {
        return false;
    }

    @Override
    public void setEvictionTimeout(long timeoutMs) {}

    @Override
    public void clearCache() {}

    @Override
    public void addObserver(Observer observer) {}

    @Override
    public void removeObserver(Observer observer) {}
}
