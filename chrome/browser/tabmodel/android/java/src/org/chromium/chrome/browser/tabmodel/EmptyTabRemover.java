// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.chrome.browser.tab.Tab;

/** Empty implementation of {@link TabRemover}. */
public class EmptyTabRemover implements TabRemover {
    @Override
    public void closeTabs(
            @NonNull TabClosureParams tabClosureParams,
            boolean allowDialog,
            @Nullable TabModelActionListener listener) {}

    @Override
    public void forceCloseTabs(@NonNull TabClosureParams tabClosureParams) {}

    @Override
    public void removeTab(
            @NonNull Tab tab, boolean allowDialog, @Nullable TabModelActionListener listener) {}
}
