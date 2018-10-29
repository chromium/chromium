// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.support_lib_glue;

import org.chromium.android_webview.AwSupportLibIsomorphic;
import org.chromium.support_lib_boundary.IsomorphicObjectBoundaryInterface;

import java.util.concurrent.Callable;

/**
 * Abstract base class for adapters whose objects are isomorphic
 */
abstract class IsomorphicAdapter implements IsomorphicObjectBoundaryInterface {
    abstract AwSupportLibIsomorphic getPeeredObject();

    @Override
    public Object getOrCreatePeer(Callable<Object> creationCallable) throws Exception {
        AwSupportLibIsomorphic peeredObject = getPeeredObject();
        if (peeredObject == null) {
            return null;
        }
        Object peer = peeredObject.getSupportLibObject();
        if (peer == null) {
            peeredObject.setSupportLibObject(peer = creationCallable.call());
        }
        return peer;
    }
}
