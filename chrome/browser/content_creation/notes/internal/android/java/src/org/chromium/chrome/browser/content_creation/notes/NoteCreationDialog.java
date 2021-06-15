// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.content_creation.notes;

import android.app.Dialog;
import android.graphics.Typeface;
import android.graphics.drawable.GradientDrawable;
import android.os.Bundle;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ImageView;
import android.widget.TextView;

import androidx.appcompat.app.AlertDialog;
import androidx.fragment.app.DialogFragment;
import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.PagerSnapHelper;
import androidx.recyclerview.widget.RecyclerView;
import androidx.recyclerview.widget.RecyclerView.OnScrollListener;
import androidx.recyclerview.widget.SnapHelper;

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
    private static final float FIRST_NOTE_PADDING_RATIO = 0.5f;

    private View mContentView;
    private String mUrlDomain;
    private String mTitle;
    private String mSelectedText;
    private int mSelectedItemIndex;

    interface NoteDialogObserver {
        void onViewCreated(View view);
    }
    private NoteDialogObserver mNoteDialogObserver;

    public void initDialog(NoteDialogObserver noteDialogObserver, String urlDomain, String title,
            String selectedText) {
        mNoteDialogObserver = noteDialogObserver;
        mUrlDomain = urlDomain;
        mTitle = title;
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

        SnapHelper snapHelper = new PagerSnapHelper();
        snapHelper.attachToRecyclerView(noteCarousel);

        noteCarousel.addOnScrollListener(new OnScrollListener() {
            @Override
            public void onScrolled(RecyclerView recyclerView, int dx, int dy) {
                LinearLayoutManager layoutManager =
                        (LinearLayoutManager) recyclerView.getLayoutManager();
                if (layoutManager.findFirstCompletelyVisibleItemPosition() < 0) return;

                int first_visible = layoutManager.findFirstCompletelyVisibleItemPosition();
                int last_visible = layoutManager.findLastCompletelyVisibleItemPosition();
                mSelectedItemIndex = (last_visible - first_visible) / 2 + first_visible;
                ((TextView) mContentView.findViewById(R.id.title))
                        .setText(carouselItems.get(mSelectedItemIndex)
                                         .model.get(NoteProperties.TEMPLATE)
                                         .localizedName);
            }
        });
    }

    /**
     * Returns the index of the currently selected item.
     */
    public int getSelectedItemIndex() {
        return this.mSelectedItemIndex;
    }

    /**
     * Returns the View instance for a note at |index|.
     */
    public View getNoteViewAt(int index) {
        RecyclerView noteCarousel = mContentView.findViewById(R.id.note_carousel);
        LinearLayoutManager layoutManager = (LinearLayoutManager) noteCarousel.getLayoutManager();
        View noteContainerView = layoutManager.findViewByPosition(index);
        return noteContainerView == null ? null : noteContainerView.findViewById(R.id.background);
    }

    private void bindCarouselItem(PropertyModel model, ViewGroup parent, PropertyKey propertyKey) {
        NoteTemplate template = model.get(NoteProperties.TEMPLATE);

        View background = parent.findViewById(R.id.background);
        template.mainBackground.apply(background);
        background.setClipToOutline(true);

        Typeface typeface = model.get(NoteProperties.TYPEFACE);
        TextView noteText = (TextView) parent.findViewById(R.id.text);
        noteText.setTypeface(typeface);
        template.textStyle.apply(noteText, mSelectedText);

        if (template.contentBackground != null) {
            template.contentBackground.apply(noteText);
        } else {
            GradientDrawable drawable = (GradientDrawable) noteText.getBackground();
            drawable.mutate();
            drawable.setColor(null);
            drawable.setColors(null);
        }

        TextView footerLink = (TextView) parent.findViewById(R.id.footer_link);
        TextView footerTitle = (TextView) parent.findViewById(R.id.footer_title);
        ImageView footerIcon = (ImageView) parent.findViewById(R.id.footer_icon);
        footerLink.setText(mUrlDomain);
        footerTitle.setText(mTitle);

        template.footerStyle.apply(footerLink, footerTitle, footerIcon);

        setLeftPadding(model.get(NoteProperties.IS_FIRST), parent.findViewById(R.id.item));
    }

    // Adjust the left padding for carousel items, so that the first item is centered and the
    // following item is slightlight peaking from the right. For that, set left padding exactly
    // what is needed to push the first item to the center, but set a smaller padding for the
    // following items.
    private void setLeftPadding(boolean isFirst, View itemView) {
        int dialogWidth = mContentView.getWidth();
        int templateWidth = getActivity().getResources().getDimensionPixelSize(R.dimen.note_width);
        int paddingLeft =
                getActivity().getResources().getDimensionPixelSize(R.dimen.note_side_padding);
        if (isFirst) {
            paddingLeft = (int) ((dialogWidth - templateWidth) * FIRST_NOTE_PADDING_RATIO + 0.5f);
        }
        itemView.setPadding(paddingLeft, itemView.getPaddingTop(), itemView.getPaddingRight(),
                itemView.getPaddingBottom());
    }
}