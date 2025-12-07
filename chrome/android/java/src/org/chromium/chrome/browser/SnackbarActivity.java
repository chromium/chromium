// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import android.os.Bundle;
import android.view.View;
import android.view.ViewGroup;

import androidx.annotation.CallSuper;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager.SnackbarManageable;

/**
 * Base Activity class that is used for lighter-weight standalone Activities that rely on the native
 * library and need to show Snackbars.
 *
 * For heavier Activities that show web content, use ChromeActivity instead to get asynchronous
 * loading of the native libraries.
 */
@NullMarked
public abstract class SnackbarActivity extends SynchronousInitializationActivity
        implements SnackbarManageable {

    private SnackbarManager mSnackbarManager;

    @CallSuper
    @Override
    protected void onCreateInternal(@Nullable Bundle savedInstanceState) {
        super.onCreateInternal(savedInstanceState);
        // TODO(crbug.com/399495650): Add render tests for snackbar padding in edge-to-edge mode.
        mSnackbarManager = new SnackbarManager(this, getContentView(), null);
    }

    @Override
    public void setContentView(int layoutResId) {
        super.setContentView(layoutResId);
        mSnackbarManager.pushParentViewToOverrideStack(getContentView());
    }

    @Override
    public void setContentView(View view) {
        super.setContentView(view);
        mSnackbarManager.pushParentViewToOverrideStack(getContentView());
    }

    @Override
    public void setContentView(View view, ViewGroup.LayoutParams params) {
        super.setContentView(view, params);
        mSnackbarManager.pushParentViewToOverrideStack(getContentView());
    }

    @Override
    public SnackbarManager getSnackbarManager() {
        return mSnackbarManager;
    }
}
