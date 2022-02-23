// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import android.content.Intent;
import android.os.Bundle;

import androidx.appcompat.app.AppCompatActivity;

import org.chromium.base.IntentUtils;
import org.chromium.chrome.browser.share.ShareHelper;
import org.chromium.chrome.browser.share.ShareRegistrationCoordinator;

/**
 * {@code ChromeActivityAccessor} is the base class for share options, which
 * are activities that are shown in the share chooser. Activities subclassing
 * ChromeAccessorActivity need to:
 * - Override {@link #getShareAction()}.
 * - Register to receive that share action in {@link ShareRegistrationCoordinator).
 */
public abstract class ChromeAccessorActivity extends AppCompatActivity {
    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        try {
            Intent intent = getIntent();
            if (intent == null) return;
            if (!Intent.ACTION_SEND.equals(intent.getAction())) return;
            if (!IntentUtils.safeHasExtra(intent, ShareHelper.EXTRA_TASK_ID)) return;

            ShareRegistrationCoordinator.onShareActionChosen(
                    intent.getIntExtra(ShareHelper.EXTRA_TASK_ID, 0), getShareAction());
        } finally {
            finish();
        }
    }

    /**
     * Return a unique action string which is used to register the action handler  in
     * {@link ShareRegistrationCoordinator}. Usually, the best option is to use the stringified
     * class name with the "ShareAction" postfix.
     */
    protected abstract String getShareAction();
}
