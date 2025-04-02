// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import org.chromium.base.Callback;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.tab.Tab;

/** Empty implementation of {@link TabRemover}. */
@NullMarked
public class EmptyTabRemover implements TabRemover {
    @Override
    public void closeTabs(
            TabClosureParams tabClosureParams,
            boolean allowDialog,
            @Nullable TabModelActionListener listener) {}

    @Override
    public void prepareCloseTabs(
            TabClosureParams tabClosureParams,
            boolean allowDialog,
            @Nullable TabModelActionListener listener,
            Callback<TabClosureParams> onPreparedCallback) {}

    @Override
    public void forceCloseTabs(TabClosureParams tabClosureParams) {}

    @Override
    public void removeTab(
            Tab tab, boolean allowDialog, @Nullable TabModelActionListener listener) {}
}
