// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.layouts;

import android.content.Context;
import android.content.ContextWrapper;
import android.content.res.Resources;
import android.test.mock.MockResources;

/**
 * This is the minimal {@link Context} needed by the {@link LayoutManager} to be working properly.
 * It points to a {@link MockResources} for anything that is based on xml configurations. For
 * everything else the standard provided Context should be sufficient.
 */
public class MockContextForLayout extends ContextWrapper {
    private final MockResourcesForLayout mResources;
    private final Resources.Theme mTheme;

    public MockContextForLayout(Context validContext) {
        super(validContext);
        mResources = new MockResourcesForLayout(validContext.getResources());
        mTheme = mResources.newTheme();
    }

    @Override
    public Resources getResources() {
        return mResources;
    }

    @Override
    public Context getApplicationContext() {
        return this;
    }

    @Override
    public Resources.Theme getTheme() {
        return mTheme;
    }
}
