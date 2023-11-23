// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.test.util;

import android.content.ComponentCallbacks;
import android.content.Context;
import android.content.ContextWrapper;

/** ContextWrapper that can be used to wrap Application. */
public class ApplicationContextWrapper extends ContextWrapper {
    public ApplicationContextWrapper(Context base) {
        super(base);
    }

    @Override
    public Context getApplicationContext() {
        // Prevent calls to getApplicationContext() from escaping our Application wrapper.
        return this;
    }

    @Override
    public void registerComponentCallbacks(ComponentCallbacks callback) {
        // Base implmementation calls getApplicationContext, so need to explicitly circumvent our
        // no-op'ing getApplicationContext().
        getBaseContext().registerComponentCallbacks(callback);
    }

    @Override
    public void unregisterComponentCallbacks(ComponentCallbacks callback) {
        // Base implmementation calls getApplicationContext, so need to explicitly circumvent our
        // no-op'ing getApplicationContext().
        getBaseContext().unregisterComponentCallbacks(callback);
    }
}
