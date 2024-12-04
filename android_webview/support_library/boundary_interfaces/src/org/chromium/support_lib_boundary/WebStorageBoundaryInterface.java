// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.support_lib_boundary;

import androidx.annotation.NonNull;

import java.util.concurrent.Executor;

public interface WebStorageBoundaryInterface {

    void deleteBrowsingData(@NonNull Executor callbackExecutor, @NonNull Runnable doneCallback);

    String deleteBrowsingDataForSite(
            @NonNull String domainOrUrl,
            @NonNull Executor callbackExecutor,
            @NonNull Runnable doneCallback);
}
