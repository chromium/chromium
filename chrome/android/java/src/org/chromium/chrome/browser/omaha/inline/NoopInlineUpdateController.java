// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omaha.inline;

import android.app.Activity;

import androidx.annotation.Nullable;

import org.chromium.base.task.PostTask;
import org.chromium.chrome.browser.omaha.UpdateStatusProvider;
import org.chromium.content_public.browser.UiThreadTaskTraits;

/**
 * An update controller that does nothing. This is used if the inline update experiment has
 * not been enabled.
 */
class NoopInlineUpdateController implements InlineUpdateController {
    NoopInlineUpdateController(Runnable callback) {
        // Do a one-off post since the state will never change.
        PostTask.postTask(UiThreadTaskTraits.DEFAULT, callback);
    }

    @Override
    public void setEnabled(boolean enabled) {}

    @Override
    public @Nullable @UpdateStatusProvider.UpdateState Integer getStatus() {
        return UpdateStatusProvider.UpdateState.NONE;
    }

    @Override
    public void startUpdate(Activity activity) {}

    @Override
    public void completeUpdate() {}
}
