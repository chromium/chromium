// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browserservices.ui.view;

import static org.chromium.chrome.browser.browserservices.ui.TrustedWebActivityModel.DISCLOSURE_EVENTS_CALLBACK;
import static org.chromium.chrome.browser.browserservices.ui.TrustedWebActivityModel.DISCLOSURE_STATE;
import static org.chromium.chrome.browser.browserservices.ui.TrustedWebActivityModel.DISCLOSURE_STATE_NOT_SHOWN;
import static org.chromium.chrome.browser.browserservices.ui.TrustedWebActivityModel.DISCLOSURE_STATE_SHOWN;

import android.content.res.Resources;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.browserservices.ui.TrustedWebActivityModel;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.lifecycle.StartStopWithNativeObserver;
import org.chromium.chrome.browser.ui.messages.snackbar.Snackbar;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyObservable;

import java.util.function.Supplier;

/**
 * Shows the Trusted Web Activity disclosure when appropriate and notifies of its acceptance.
 *
 * <p>Thread safety: All methods on this class should be called on the UI thread.
 */
@NullMarked
public class DisclosureInfobar
        implements PropertyObservable.PropertyObserver<PropertyKey>, StartStopWithNativeObserver {
    private final Resources mResources;
    private final Supplier<SnackbarManager> mSnackbarManagerSupplier;
    private final TrustedWebActivityModel mModel;

    /**
     * A {@link SnackbarManager.SnackbarController} that records the users acceptance of the
     * "Running in Chrome" disclosure.
     *
     * <p>It is also used as a key to for our snackbar so we can dismiss it when the user navigates
     * to a page where they don't need to show the disclosure.
     */
    private final SnackbarManager.SnackbarController mSnackbarController =
            new SnackbarManager.SnackbarController() {
                /** To be called when the user accepts the Running in Chrome disclosure. */
                @Override
                public void onAction(@Nullable Object actionData) {
                    mModel.get(DISCLOSURE_EVENTS_CALLBACK).onDisclosureAccepted();
                }
            };

    public DisclosureInfobar(
            Resources resources,
            Supplier<SnackbarManager> snackbarManagerSupplier,
            TrustedWebActivityModel model,
            ActivityLifecycleDispatcher lifecycleDispatcher) {
        mResources = resources;
        mSnackbarManagerSupplier = snackbarManagerSupplier;
        mModel = model;
        mModel.addObserver(this);
        lifecycleDispatcher.register(this);
    }

    @Override
    public void onPropertyChanged(
            PropertyObservable<PropertyKey> source, @Nullable PropertyKey propertyKey) {
        if (propertyKey != DISCLOSURE_STATE) return;

        switch (mModel.get(DISCLOSURE_STATE)) {
            case DISCLOSURE_STATE_SHOWN:
                showIfNeeded();
                break;
            case DISCLOSURE_STATE_NOT_SHOWN:
                mSnackbarManagerSupplier.get().dismissSnackbars(mSnackbarController);
                break;
        }
    }

    @Override
    public void onStartWithNative() {
        // SnackbarManager removes all snackbars when Chrome goes to background. Restore if needed.
        showIfNeeded();
    }

    @Override
    public void onStopWithNative() {}

    /**
     * Creates the Infobar/Snackbar to show. The override of this method in
     * {@link DisclosureSnackbar} may return {@code null}, if the infobar is already shown.
     */
    protected @Nullable Snackbar makeRunningInChromeInfobar(
            SnackbarManager.SnackbarController controller) {
        String title = mResources.getString(R.string.twa_running_in_chrome);
        int type = Snackbar.TYPE_PERSISTENT;

        int code = Snackbar.UMA_TWA_PRIVACY_DISCLOSURE;

        String action = mResources.getString(R.string.ok);
        return Snackbar.make(title, mSnackbarController, type, code)
                .setAction(action, null)
                .setDefaultLines(false);
    }

    public void showIfNeeded() {
        if (mModel.get(DISCLOSURE_STATE) != DISCLOSURE_STATE_SHOWN) return;

        Snackbar snackbar = makeRunningInChromeInfobar(mSnackbarController);
        if (snackbar == null) {
            return;
        }

        mSnackbarManagerSupplier.get().showSnackbar(snackbar);
        mModel.get(DISCLOSURE_EVENTS_CALLBACK).onDisclosureShown();
    }
}
