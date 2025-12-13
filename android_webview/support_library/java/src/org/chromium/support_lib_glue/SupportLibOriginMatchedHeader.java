// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.support_lib_glue;

import org.chromium.android_webview.AwOriginMatchedHeader;
import org.chromium.support_lib_boundary.OriginMatchedHeaderBoundaryInterface;

import java.util.Set;

/**
 * The support-lib glue implementation for OriginMatchedHeader delegates all the calls to {@link
 * AwOriginMatchedHeader}.
 */
public class SupportLibOriginMatchedHeader implements OriginMatchedHeaderBoundaryInterface {

    private final AwOriginMatchedHeader mDelegate;

    public SupportLibOriginMatchedHeader(AwOriginMatchedHeader delegate) {
        this.mDelegate = delegate;
    }

    @Override
    public String getName() {
        return mDelegate.getName();
    }

    @Override
    public String getValue() {
        return mDelegate.getValue();
    }

    @Override
    public Set<String> getRules() {
        return mDelegate.getRules();
    }
}
