// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.support_lib_boundary;

import java.util.concurrent.Callable;

/**
 * Boundary interface to be implemented by any type that needs to maintain an isomorphism (i.e.
 * a 1:1 mapping) between the support library objects and their corresponding internal WebView
 * objects.
 */
public interface IsomorphicObjectBoundaryInterface {
    /**
     * Get the peer object associated with this object.
     *
     * One of the pair of classes for which the corresponding objects
     * need to maintain a lazy 1:1 mapping between support library and
     * webview should implement this interface.
     *
     * The mapping is lazy in the sense that one object in the pair
     * (the peer) may be created at an arbitrary point after the other,
     * but once both objects exist their lifetimes become tied and the
     * 1:1 mapping is maintained.
     *
     * Whether this interface should be implemented by the support library
     * class or the webview class depends on which side can be created and
     * exist independently. For example, AwRenderProcess objects are created
     * by WebView before being passed to the support library, and thus
     * SupportLibWebViewRendererAdapter should implement this interface.
     *
     * @param creationCallable A callable that can be used to construct an appropriate peer
     *                         object, if one is required.
     * @return The peer object associated with this object, which either exists already, or has
     *         been freshly created and recorded.
     */
    Object getOrCreatePeer(Callable<Object> creationCallable);
}
