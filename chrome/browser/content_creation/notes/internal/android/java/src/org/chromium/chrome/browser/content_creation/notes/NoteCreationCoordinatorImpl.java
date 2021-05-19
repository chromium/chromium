// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.content_creation.notes;

import android.app.Activity;
import android.view.View;

import androidx.fragment.app.FragmentActivity;

import org.chromium.chrome.browser.content_creation.notes.fonts.GoogleFontService;
import org.chromium.chrome.browser.content_creation.notes.top_bar.TopBarCoordinator;
import org.chromium.components.content_creation.notes.NoteService;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;

/**
 * Responsible for notes main UI and its subcomponents.
 */
public class NoteCreationCoordinatorImpl implements NoteCreationCoordinator {
    private final Activity mActivity;
    private final ModelList mListModel;
    private final NoteCreationMediator mMediator;
    private final NoteCreationDialog mDialog;

    private TopBarCoordinator mTopBarCoordinator;

    public NoteCreationCoordinatorImpl(
            Activity activity, NoteService noteService, String selectedText) {
        mActivity = activity;

        mListModel = new ModelList();

        mMediator =
                new NoteCreationMediator(mListModel, new GoogleFontService(mActivity), noteService);

        mDialog = new NoteCreationDialog();
        mDialog.initDialog(this::onViewCreated, selectedText);
    }

    @Override
    public void showDialog() {
        FragmentActivity fragmentActivity = (FragmentActivity) mActivity;
        mDialog.show(fragmentActivity.getSupportFragmentManager(), null);
    }

    /**
     * Dismiss the main dialog.
     */
    public void dismissDialog() {
        mDialog.dismiss();
    }

    /**
     * Initializes the top bar after the parent view is ready.
     * @param view A {@link View} to corresponding to the parent view for the top bar.
     */
    private void onViewCreated(View view) {
        mTopBarCoordinator = new TopBarCoordinator(mActivity, view, this::dismissDialog);
        mDialog.createRecyclerViews(mListModel);
    }
}
