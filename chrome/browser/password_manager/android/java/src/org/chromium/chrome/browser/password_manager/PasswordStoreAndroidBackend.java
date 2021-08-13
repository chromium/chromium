// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_manager;

import org.chromium.base.Callback;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.components.sync.protocol.ListPasswordsResult;

/**
 * Java implementation of the backend. Uses a Google Mobile Services API to fulfill password store
 * tasks. All methods are expected to respond asynchronously to callbacks.
 */
class PasswordStoreAndroidBackend {
    PasswordStoreAndroidBackend() {}

    public void getAllLogins(Callback<byte[]> loginsReply) {
        // TODO(crbug.com/1229654): Call actual API.
        PostTask.postTask(TaskTraits.THREAD_POOL_USER_VISIBLE, () -> {
            loginsReply.onResult(ListPasswordsResult.getDefaultInstance().toByteArray());
        });
    }
}
