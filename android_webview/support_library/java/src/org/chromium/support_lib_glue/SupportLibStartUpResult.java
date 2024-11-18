// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.support_lib_glue;

import org.chromium.support_lib_boundary.WebViewStartUpResultBoundaryInterface;

class SupportLibStartUpResult implements WebViewStartUpResultBoundaryInterface {
    private Long mTotalTimeInUiThreadMillis;

    SupportLibStartUpResult() {}

    void setTotalTimeInUiThreadMillis(Long time) {
        mTotalTimeInUiThreadMillis = time;
    }

    @Override
    public Long getTotalTimeInUiThreadMillis() {
        return mTotalTimeInUiThreadMillis;
    }
}
