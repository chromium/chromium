// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.enterprise.util;

import org.chromium.base.Callback;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;

import java.util.ArrayList;
import java.util.List;

/**
 * Simple EnterpriseInfo that always invokes callbacks asynchronously. When created, it will store
 * any requests for enterprise info, until {@link FakeEnterpriseInfo#initialize(OwnedState)} is
 * called, at which point it will run all old and new callbacks.
 */
public class FakeEnterpriseInfo extends EnterpriseInfo {
    private final List<Callback<OwnedState>> mCallbackList = new ArrayList<>();

    // Track initialized state separately from if mOwnedState is null or not. Some tests want to
    // return a null OwnedState, which is what what EnterpriseInfo does on errors.
    private OwnedState mOwnedState;
    private boolean mInitialized;

    @Override
    public void getDeviceEnterpriseInfo(Callback<OwnedState> callback) {
        if (mInitialized) {
            PostTask.postTask(TaskTraits.UI_DEFAULT, () -> callback.onResult(mOwnedState));
        } else {
            mCallbackList.add(callback);
        }
    }

    @Override
    public void logDeviceEnterpriseInfo() {}

    /** Sets the owned state and posts tasks to run any pending callbacks. */
    public void initialize(OwnedState ownedState) {
        mInitialized = true;
        mOwnedState = ownedState;
        for (Callback<OwnedState> callback : mCallbackList) {
            PostTask.postTask(TaskTraits.UI_DEFAULT, () -> callback.onResult(mOwnedState));
        }
        mCallbackList.clear();
    }
}
