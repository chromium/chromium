// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.common;

import org.chromium.base.Callback;
import org.chromium.build.annotations.NullMarked;
import org.chromium.url.GURL;

/**
 * This interface provides a means for checking whether to load or block a url for a supervised
 * user.
 */
@NullMarked
public interface AwSupervisedUserUrlClassifierDelegate {
    /**
     * Checks whether {@code requestUrl} should be restricted or should be allowed. The result is
     * returned via the callback.
     *
     * <p>Be aware that the callback may be invoked on any thread.
     *
     * <p>callback.onResult(false) - indicates the url should be loaded as normal.
     * callback.onResult(true) - indicates the url should be blocked and an appropriate error page
     * shown instead.
     */
    void shouldBlockUrl(GURL requestUrl, final Callback<Boolean> callback);

    /**
     * Checks whether restricted content blocking should apply to this user. The result is returned
     * via the callback.
     *
     * <p>Be aware that the callback may be invoked on any thread.
     *
     * <p>callback.onResult(false) - indicates the user does not require restricted content
     * blocking. callback.onResult(true) - indicates the user requires restricted content blocking.
     */
    void needsRestrictedContentBlocking(final Callback<Boolean> callback);
}
