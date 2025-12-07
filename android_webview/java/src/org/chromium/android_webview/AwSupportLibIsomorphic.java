// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

import java.util.concurrent.Callable;

import javax.annotation.concurrent.GuardedBy;

/**
 * Abstract base class for objects that are expected to be isomorphic (i.e. have a lazy 1:1 mapping
 * for its entire lifetime) with a support library object.
 */
@NullMarked
public abstract class AwSupportLibIsomorphic {

    private final Object mSupportLibObjectLock = new Object();

    @GuardedBy("mSupportLibObjectLock")
    @Nullable
    private Object mSupportLibObject;

    public Object getOrCreateSupportLibObject(Callable<Object> creationCallable) {
        synchronized (mSupportLibObjectLock) {
            if (mSupportLibObject != null) {
                return mSupportLibObject;
            }
            try {
                mSupportLibObject = creationCallable.call();
            } catch (Exception e) {
                throw new RuntimeException("Could not create peered object", e);
            }
            return mSupportLibObject;
        }
    }
}
