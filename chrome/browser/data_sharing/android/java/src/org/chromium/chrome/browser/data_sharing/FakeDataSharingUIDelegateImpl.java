// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.data_sharing;

import androidx.annotation.Nullable;

import org.chromium.build.annotations.NullMarked;
import org.chromium.components.data_sharing.DataSharingUIDelegate;
import org.chromium.components.data_sharing.configs.DataSharingAvatarBitmapConfig;
import org.chromium.components.data_sharing.configs.DataSharingCreateUiConfig;
import org.chromium.components.data_sharing.configs.DataSharingJoinUiConfig;
import org.chromium.components.data_sharing.configs.DataSharingManageUiConfig;
import org.chromium.components.data_sharing.configs.DataSharingRuntimeDataConfig;
import org.chromium.url.GURL;

/** Fake implementation of {@link DataSharingUIDelegate} */
@NullMarked
public class FakeDataSharingUIDelegateImpl implements DataSharingUIDelegate {

    private @Nullable Runnable mShowJoinFlowRunnable;
    private @Nullable Runnable mShowCreateFlowRunnable;
    private @Nullable Runnable mShowManageFlowRunnable;

    public FakeDataSharingUIDelegateImpl() {}

    @Override
    public void getAvatarBitmap(DataSharingAvatarBitmapConfig avatarBitmapConfig) {}

    @Override
    public void handleShareURLIntercepted(GURL url) {}

    @Override
    public String showCreateFlow(DataSharingCreateUiConfig createUiConfig) {
        if (mShowCreateFlowRunnable != null) mShowCreateFlowRunnable.run();
        return "";
    }

    @Override
    public String showJoinFlow(DataSharingJoinUiConfig joinUiConfig) {
        if (mShowJoinFlowRunnable != null) mShowJoinFlowRunnable.run();
        return "";
    }

    @Override
    public String showManageFlow(DataSharingManageUiConfig manageUiConfig) {
        if (mShowManageFlowRunnable != null) mShowManageFlowRunnable.run();
        return "";
    }

    @Override
    public void updateRuntimeData(
            @Nullable String sessionId, DataSharingRuntimeDataConfig runtimeData) {}

    @Override
    public void destroyFlow(String sessionId) {}

    /* Set a runnable to be called when showJoinFlow() is called. */
    public void setShowJoinFlowRunnable(Runnable runnable) {
        mShowJoinFlowRunnable = runnable;
    }

    /* Set a runnable to be called when showCreateFlow() is called. */
    public void setShowCreateFlowRunnable(Runnable runnable) {
        mShowCreateFlowRunnable = runnable;
    }

    /* Set a runnable to be called when showManageFlow() is called. */
    public void setShowManageFlowRunnable(Runnable runnable) {
        mShowManageFlowRunnable = runnable;
    }
}
