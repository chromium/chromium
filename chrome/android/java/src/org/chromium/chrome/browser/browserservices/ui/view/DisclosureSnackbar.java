// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browserservices.ui.view;

import android.content.res.Resources;

import androidx.annotation.Nullable;

import dagger.Lazy;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.browserservices.ui.TrustedWebActivityModel;
import org.chromium.chrome.browser.dependency_injection.ActivityScope;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.ui.messages.snackbar.Snackbar;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;

import javax.inject.Inject;

/**
 * Implements the new "Running in Chrome" Snackbar behavior, taking over from {@link
 * DisclosureInfobar}.
 *
 * <p>As opposed to {@link DisclosureInfobar} the Snackbar shown by this class is transient (lasting
 * 7 seconds) and only is shown at first launch (not on subsequent navigation back to the verified
 * origin).
 *
 * <p>Thread safety: All methods should be called on the UI thread.
 */
@ActivityScope
public class DisclosureSnackbar extends DisclosureInfobar {
    // TODO(crbug.com/40125323): Once this feature is enabled by default, remove
    // TrustedWebActivityDisclosureView and simplify this class.

    private static final int DURATION_MS = 7000;

    private final Resources mResources;
    private final TrustedWebActivityModel mModel;

    private boolean mShown;

    @Inject
    DisclosureSnackbar(
            Resources resources,
            Lazy<SnackbarManager> snackbarManager,
            TrustedWebActivityModel model,
            ActivityLifecycleDispatcher lifecycleDispatcher) {
        super(resources, snackbarManager, model, lifecycleDispatcher);
        mResources = resources;
        mModel = model;
    }

    @Override
    protected @Nullable Snackbar makeRunningInChromeInfobar(
            SnackbarManager.SnackbarController controller) {
        if (mShown) return null;
        mShown = true;

        String title = mResources.getString(R.string.twa_running_in_chrome);

        int type = Snackbar.TYPE_ACTION;
        int code = Snackbar.UMA_TWA_PRIVACY_DISCLOSURE_V2;

        String action = mResources.getString(R.string.got_it);

        return Snackbar.make(title, controller, type, code)
                .setAction(action, null)
                .setDuration(DURATION_MS)
                .setSingleLine(false);
    }
}
