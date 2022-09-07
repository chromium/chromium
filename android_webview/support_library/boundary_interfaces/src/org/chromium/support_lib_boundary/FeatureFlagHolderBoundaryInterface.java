// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.support_lib_boundary;

/**
 * Boundary interface to be implemented by any type which is constructed on the support library
 * side (such as callback classes). This interface is a way for the instance to declare which
 * {@link org.chromium.support_lib_boundary.util.Features} it supports (this may vary between
 * instances if the app uses multiple versions of the support library).
 *
 * This need only be implemented by objects created on the support library side, since we know any
 * objects created on the chromium side have the same feature list as the WebView APK itself (as
 * returned by {@link WebViewProviderFactoryBoundaryInterface#getSupportedFeatures}).
 */
public interface FeatureFlagHolderBoundaryInterface {
    /**
     * Indicate the list of {@link org.chromium.support_lib_boundary.util.Features} supported by
     * this object.
     *
     * @return The supported features.
     */
    String[] getSupportedFeatures();
}
