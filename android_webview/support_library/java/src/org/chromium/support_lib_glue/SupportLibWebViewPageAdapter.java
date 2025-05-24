// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.support_lib_glue;

import org.chromium.android_webview.AwPage;
import org.chromium.android_webview.AwSupportLibIsomorphic;
import org.chromium.android_webview.common.Lifetime;
import org.chromium.support_lib_boundary.WebViewPageBoundaryInterface;

/**
 * Adapter between WebViewPageBoundaryInterface and Page.
 *
 * <p>Once created, instances are kept alive by the peer Page.
 */
@Lifetime.Temporary
class SupportLibWebViewPageAdapter extends IsomorphicAdapter
        implements WebViewPageBoundaryInterface {
    private final AwPage mPage;

    SupportLibWebViewPageAdapter(AwPage page) {
        mPage = page;
    }

    @Override
    AwSupportLibIsomorphic getPeeredObject() {
        return mPage;
    }
}
