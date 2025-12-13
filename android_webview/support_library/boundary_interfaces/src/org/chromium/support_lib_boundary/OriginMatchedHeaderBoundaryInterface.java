// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.support_lib_boundary;

import java.util.Set;

/** Boundary interface for {@link org.chromium.android_webview.AwOriginMatchedHeader}. */
public interface OriginMatchedHeaderBoundaryInterface {

    String getName();

    String getValue();

    Set<String> getRules();
}
