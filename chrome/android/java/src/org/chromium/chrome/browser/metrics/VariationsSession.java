// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.metrics;

import org.chromium.base.Callback;
import org.chromium.base.annotations.NativeMethods;

/**
 * Sets up communication with the VariationsService. This is primarily used for
 * triggering seed fetches on application startup.
 */
public class VariationsSession {
    private boolean mRestrictModeFetchStarted;
    private String mRestrictMode;

    /**
     * Triggers to the native VariationsService that the application has entered the foreground.
     */
    public void start() {
        // If |mRestrictModeFetchStarted| is true and |mRestrictMode| is null, then async
        // initialization is in progress and VariationsSessionJni.get().startVariationsSession()
        // will be called when it completes.
        if (mRestrictModeFetchStarted && mRestrictMode == null) {
            return;
        }

        mRestrictModeFetchStarted = true;
        getRestrictModeValue(new Callback<String>() {
            @Override
            public void onResult(String restrictMode) {
                VariationsSessionJni.get().startVariationsSession(
                        VariationsSession.this, mRestrictMode);
            }
        });
    }

    /**
     * Asynchronously returns the value of the "restrict" URL param that the variations service
     * should use for variation seed requests. Public version that can be called externally.
     * Uses the protected version (that could be overridden by subclasses) to actually get the
     * value and also sets that value internally when retrieved.
     * @param callback Callback that will be called with the param value when available.
     */
    public final void getRestrictModeValue(final Callback<String> callback) {
        // If |mRestrictMode| is not null, the value has already been fetched and so it can
        // simply be provided to the callback.
        if (mRestrictMode != null) {
            callback.onResult(mRestrictMode);
            return;
        }
        getRestrictMode(new Callback<String>() {
            @Override
            public void onResult(String restrictMode) {
                assert restrictMode != null;
                mRestrictMode = restrictMode;
                callback.onResult(restrictMode);
            }
        });
    }

    /**
     * Asynchronously returns the value of the "restrict" URL param that the variations service
     * should use for variation seed requests. This can be overriden by subclass to provide actual
     * restrict values, which must not be null.
     */
    protected void getRestrictMode(Callback<String> callback) {
        callback.onResult("");
    }

    /**
     * @return The latest country according to the current variations state. Null if not known.
     */
    public String getLatestCountry() {
        return VariationsSessionJni.get().getLatestCountry(this);
    }

    @NativeMethods
    interface Natives {
        void startVariationsSession(VariationsSession caller, String restrictMode);
        String getLatestCountry(VariationsSession caller);
    }
}
