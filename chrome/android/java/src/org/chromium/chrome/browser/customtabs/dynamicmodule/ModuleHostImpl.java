// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs.dynamicmodule;

import android.content.Context;

/**
 * The implementation of {@link IModuleHost}.
 */
public class ModuleHostImpl extends BaseModuleHost {
    private static final int VERSION = 5;
    private static final int MINIMUM_MODULE_VERSION = 1;

    private final Context mApplicationContext;
    private final Context mModuleContext;

    public ModuleHostImpl(Context applicationContext, Context moduleContext) {
        mApplicationContext = applicationContext;
        mModuleContext = moduleContext;
    }

    @Override
    public IObjectWrapper getHostApplicationContext() {
        return ObjectWrapper.wrap(mApplicationContext);
    }

    @Override
    public IObjectWrapper getModuleContext() {
        return ObjectWrapper.wrap(mModuleContext);
    }

    @Override
    public int getHostVersion() {
        return VERSION;
    }

    @Override
    public int getMinimumModuleVersion() {
        return MINIMUM_MODULE_VERSION;
    }
}
