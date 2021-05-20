// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.content_creation.notes;

import android.app.Dialog;
import android.os.Bundle;
import android.view.Gravity;
import android.view.View;
import android.view.ViewGroup;
import android.widget.TextView;

import androidx.appcompat.app.AlertDialog;
import androidx.fragment.app.DialogFragment;
import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.RecyclerView;

import org.chromium.chrome.browser.content_creation.internal.R;
import org.chromium.components.content_creation.notes.models.NoteTemplate;
import org.chromium.ui.modelutil.LayoutViewBuilder;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.SimpleRecyclerViewAdapter;

/**
 * Dialog for the note creation.
 */
public class NoteCreationDialog extends DialogFragment {
    private View mContentView;
    private String mSelectedText;

    interface NoteDialogObserver {
        void onViewCreated(View view);
    }
    private NoteDialogObserver mNoteDialogObserver;

    public void initDialog(NoteDialogObserver noteDialogObserver, String selectedText) {
        mNoteDialogObserver = noteDialogObserver;
        mSelectedText = selectedText;
    }

    @Override
    public Dialog onCreateDialog(Bundle savedInstanceState) {
        AlertDialog.Builder builder =
                new AlertDialog.Builder(getActivity(), R.style.Theme_Chromium_Fullscreen);
        mContentView = getActivity().getLayoutInflater().inflate(R.layout.creation_dialog, null);
        builder.setView(mContentView);

        if (mNoteDialogObserver != null) mNoteDialogObserver.onViewCreated(mContentView);

        return builder.create();
    }

    /*
     * Creates a note carousel for the provided PropertyModels.
     *
     * @param activity The activity the share sheet belongs to.
     * @param carouselItems The PropertyModels used to build the top row.
     */
    public void createRecyclerViews(ModelList carouselItems) {
        RecyclerView noteCarousel = mContentView.findViewById(R.id.note_carousel);

        SimpleRecyclerViewAdapter adapter = new SimpleRecyclerViewAdapter(carouselItems);
        adapter.registerType(NoteProperties.NOTE_VIEW_TYPE,
                new LayoutViewBuilder(R.layout.carousel_item), this::bindCarouselItem);
        noteCarousel.setAdapter(adapter);
        LinearLayoutManager layoutManager =
                new LinearLayoutManager(getActivity(), LinearLayoutManager.HORIZONTAL, false);
        noteCarousel.setLayoutManager(layoutManager);
    }

    private void bindCarouselItem(PropertyModel model, ViewGroup parent, PropertyKey propertyKey) {
        NoteTemplate template = model.get(NoteProperties.TEMPLATE);

        View background = parent.findViewById(R.id.background);
        template.mainBackground.apply(background);
        background.setClipToOutline(true);
        ((TextView) parent.findViewById(R.id.title)).setText(template.localizedName);

        TextView noteText = (TextView) parent.findViewById(R.id.text);
        noteText.setText(mSelectedText);
        noteText.setTextColor(template.textStyle.fontColor);
        noteText.setAllCaps(template.textStyle.allCaps);
        noteText.setGravity(Gravity.CENTER);
    }
}