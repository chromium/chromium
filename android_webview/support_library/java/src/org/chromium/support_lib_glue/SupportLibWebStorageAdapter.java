// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.support_lib_glue;

import static org.chromium.support_lib_glue.SupportLibWebViewChromiumFactory.recordApiCall;

import androidx.annotation.NonNull;

import org.chromium.android_webview.AwQuotaManagerBridge;
import org.chromium.support_lib_boundary.WebStorageBoundaryInterface;
import org.chromium.support_lib_glue.SupportLibWebViewChromiumFactory.ApiCall;

import java.util.concurrent.Executor;

public class SupportLibWebStorageAdapter implements WebStorageBoundaryInterface {

    private final AwQuotaManagerBridge mWebStorageImpl;

    public SupportLibWebStorageAdapter(AwQuotaManagerBridge webStorageImpl) {
        this.mWebStorageImpl = webStorageImpl;
    }

    @Override
    public void deleteBrowsingData(
            @NonNull Executor callbackExecutor, @NonNull Runnable doneCallback) {
        recordApiCall(ApiCall.WEB_STORAGE_DELETE_BROWSING_DATA);
        mWebStorageImpl.deleteBrowsingData(ignored -> callbackExecutor.execute(doneCallback));
    }

    @Override
    public String deleteBrowsingDataForSite(
            @NonNull String domainOrUrl,
            @NonNull Executor callbackExecutor,
            @NonNull Runnable doneCallback) {
        recordApiCall(ApiCall.WEB_STORAGE_DELETE_BROWSING_DATA_FOR_SITE);
        return mWebStorageImpl.deleteBrowsingDataForSite(
                domainOrUrl, ignored -> callbackExecutor.execute(doneCallback));
    }
}
