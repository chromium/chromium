// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.content_creation.notes;

import android.app.Activity;
import android.view.View;

import androidx.fragment.app.FragmentActivity;

import org.chromium.chrome.browser.content_creation.notes.top_bar.TopBarCoordinator;

/**
 * Responsible for notes main UI and its subcomponents.
 */
public class NoteCreationCoordinatorImpl implements NoteCreationCoordinator {
    private Activity mActivity;
    private NoteCreationDialog mDialog;
    private TopBarCoordinator mTopBarCoordinator;

    @Override
    public void initialize(Activity activity) {
        mActivity = activity;
        mDialog = new NoteCreationDialog();
        mDialog.initDialog(this::onViewCreated);
    }

    @Override
    public void showDialog() {
        if (mDialog != null) {
            FragmentActivity fragmentActivity = (FragmentActivity) mActivity;
            mDialog.show(fragmentActivity.getSupportFragmentManager(), null);
        }
    }

    /**
     * Dismiss the main dialog.
     */
    public void dismissDialog() {
        if (mDialog != null) mDialog.dismiss();
    }

    /**
     * Initializes the top bar after the parent view is ready.
     * @param view A {@link View} to corresponding to the parent view for the top bar.
     */
    private void onViewCreated(View view) {
        mTopBarCoordinator = new TopBarCoordinator(mActivity, view, this::dismissDialog);
    }
}
