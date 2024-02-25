// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.common;

import androidx.annotation.NonNull;

import org.chromium.base.Callback;
import org.chromium.url.GURL;

/**
 * This interface provides a means for checking whether to load or block a url
 * for a supervised user. The result is returned via the callback.
 *
 * callback.onResult(false) - indicates the url should be loaded as normal.
 * callback.onResult(true) - indicates the url should be blocked and an appropriate
 * error page shown instead.
 */
public interface AwSupervisedUserUrlClassifierDelegate {
    void shouldBlockUrl(GURL requestUrl, @NonNull final Callback<Boolean> callback);
}
