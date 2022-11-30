// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview;

/**
 * Abstract base class for objects that are expected to be isomorphic
 * (i.e. have a lazy 1:1 mapping for its entire lifetime) with a support
 * library object.
 */
public abstract class AwSupportLibIsomorphic {
    private Object mSupportLibObject;

    public Object getSupportLibObject() {
        return mSupportLibObject;
    }

    public void setSupportLibObject(Object supportLibObject) {
        assert mSupportLibObject == null;
        mSupportLibObject = supportLibObject;
    }
}
