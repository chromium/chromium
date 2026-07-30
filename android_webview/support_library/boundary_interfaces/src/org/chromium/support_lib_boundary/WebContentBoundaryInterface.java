// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.support_lib_boundary;

import android.content.Context;
import android.webkit.WebView;

import org.jspecify.annotations.NullMarked;

import java.util.function.Function;

/**
 * Boundary interface for WebContent, which represents underlying Chromium web contents state that
 * can survive moving across WebViews.
 */
@NullMarked
public interface WebContentBoundaryInterface {
    <T extends WebView> T executeViewFactory(Context baseContext, Function<Context, T> factory);

    void destroy();
}
