// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.support_lib_glue;

import org.chromium.support_lib_boundary.WebViewStartUpResultBoundaryInterface;

import java.util.ArrayList;
import java.util.List;

class SupportLibStartUpResult implements WebViewStartUpResultBoundaryInterface {
    private Long mTotalTimeInUiThreadMillis;
    private Long mMaxTimePerTaskTimeInUiThreadMillis;
    private final List<Throwable> mBlockingStartUpLocations = new ArrayList<Throwable>();
    private final List<Throwable> mAsyncStartUpLocations = new ArrayList<Throwable>();

    SupportLibStartUpResult() {}

    @Override
    public Long getTotalTimeInUiThreadMillis() {
        return mTotalTimeInUiThreadMillis;
    }

    @Override
    public Long getMaxTimePerTaskInUiThreadMillis() {
        return mMaxTimePerTaskTimeInUiThreadMillis;
    }

    @Override
    public List<Throwable> getBlockingStartUpLocations() {
        return mBlockingStartUpLocations;
    }

    @Override
    public List<Throwable> getAsyncStartUpLocations() {
        return mAsyncStartUpLocations;
    }

    void setTotalTimeInUiThreadMillis(Long time) {
        mTotalTimeInUiThreadMillis = time;
    }

    void setMaxTimePerTaskInUiThreadMillis(Long time) {
        mMaxTimePerTaskTimeInUiThreadMillis = time;
    }

    void addBlockingStartUpLocation(Throwable t) {
        mBlockingStartUpLocations.add(t);
    }

    void addAsyncStartUpLocation(Throwable t) {
        mAsyncStartUpLocations.add(t);
    }
}
