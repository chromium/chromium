// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.support_lib_glue;

import org.chromium.android_webview.AwNavigationParams;
import org.chromium.build.annotations.NullMarked;
import org.chromium.support_lib_boundary.NavigationParametersBoundaryInterface;
import org.chromium.support_lib_boundary.util.BoundaryInterfaceReflectionUtil;

import java.lang.reflect.InvocationHandler;

/** Adapter between NavigationParametersBoundaryInterface and AwNavigationParams. */
@NullMarked
public class SupportLibNavigationParametersAdapter {

    private final NavigationParametersBoundaryInterface mImpl;

    public SupportLibNavigationParametersAdapter(
            /* NavigationParams */ InvocationHandler invocationHandler) {
        mImpl =
                BoundaryInterfaceReflectionUtil.castToSuppLibClass(
                        NavigationParametersBoundaryInterface.class, invocationHandler);
    }

    public AwNavigationParams toAwNavigationParams(String url) {
        return new AwNavigationParams(
                url, mImpl.getShouldReplaceCurrentEntry(), mImpl.getAdditionalHeaders());
    }
}
