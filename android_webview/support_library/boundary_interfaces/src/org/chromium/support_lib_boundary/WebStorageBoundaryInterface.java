// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.support_lib_boundary;

import org.jspecify.annotations.NullMarked;

import java.util.concurrent.Executor;

@NullMarked
public interface WebStorageBoundaryInterface {

    void deleteBrowsingData(Executor callbackExecutor, Runnable doneCallback);

    String deleteBrowsingDataForSite(
            String domainOrUrl, Executor callbackExecutor, Runnable doneCallback);
}
