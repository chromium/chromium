// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.content_creation.notes;

import android.app.Dialog;
import android.content.res.Configuration;
import android.graphics.Typeface;
import android.os.Bundle;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewGroup.MarginLayoutParams;
import android.widget.Button;
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
import org.chromium.ui.widget.Toast;

/**
 * Dialog for the note creation.
 */
public class NoteCreationDialog extends DialogFragment {
    private static final float EXTREMITY_NOTE_PADDING_RATIO = 0.5f;

    private View mContentView;
    private String mUrlDomain;
    private String mTitle;
    private String mSelectedText;
    private int mSelectedItemIndex;
    private Toast mToast;
    private boolean mIsPublishAvailable;
    private int mNbTemplateSwitches;

    interface NoteDialogObserver {
        void onViewCreated(View view);
    }
    private NoteDialogObserver mNoteDialogObserver;

    public void initDialog(NoteDialogObserver noteDialogObserver, String urlDomain, String title,
            String selectedText, boolean isPublishAvailable) {
        mNoteDialogObserver = noteDialogObserver;
        mUrlDomain = urlDomain;
        mTitle = title;
        mSelectedText = selectedText;
        mIsPublishAvailable = isPublishAvailable;
    }

    @Override
    public Dialog onCreateDialog(Bundle savedInstanceState) {
        AlertDialog.Builder builder =
                new AlertDialog.Builder(getActivity(), R.style.Theme_Chromium_Fullscreen);
        mContentView = getActivity().getLayoutInflater().inflate(R.layout.creation_dialog, null);
        builder.setView(mContentView);

        // Push down the note title depending on screensize.
        int minTopMargin = getActivity().getResources().getDimensionPixelSize(
                R.dimen.note_title_min_top_margin);
        int screenHeight = getActivity().getResources().getDisplayMetrics().heightPixels;
        int topMarginOffset = getActivity().getResources().getDimensionPixelSize(
                R.dimen.note_title_top_margin_offset);
        View titleView = mContentView.findViewById(R.id.title);
        MarginLayoutParams params = (MarginLayoutParams) titleView.getLayoutParams();
        params.topMargin = (int) (minTopMargin + (screenHeight - topMarginOffset) * 0.15f);
        titleView.setLayoutParams(params);
        titleView.requestLayout();

        if (mIsPublishAvailable) {
            Button publishButton = (Button) mContentView.findViewById(R.id.publish);
            publishButton.setVisibility(View.VISIBLE);
        }

        if (mNoteDialogObserver != null) mNoteDialogObserver.onViewCreated(mContentView);

        return builder.create();
    }

    @Override
    public void onConfigurationChanged(Configuration newConfig) {
        super.onConfigurationChanged(newConfig);

        // There is a corner case where this function can be triggered by toggling the battery saver
        // state, resulting in all the variables being reset. The only way out is to destroy this
        // dialog to bring the user back to the web page.
        if (mTitle == null) {
            onDestroyView();
            return;
        }

        // Force recycler view to redraw to recalculate the left/right paddings for first/last
        // items.
        RecyclerView noteCarousel = mContentView.findViewById(R.id.note_carousel);
        noteCarousel.getAdapter().notifyDataSetChanged();
        centerCurrentNote();
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
                unFocus(mSelectedItemIndex);
                mSelectedItemIndex = (last_visible - first_visible) / 2 + first_visible;
                ((TextView) mContentView.findViewById(R.id.title))
                        .setText(carouselItems.get(mSelectedItemIndex)
                                         .model.get(NoteProperties.TEMPLATE)
                                         .localizedName);
                focus(mSelectedItemIndex);
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
        if (layoutManager == null) return null;
        View noteContainerView = layoutManager.findViewByPosition(index);
        return noteContainerView == null ? null : noteContainerView.findViewById(R.id.item);
    }

    /**
     * Returns the number of template switches the user did.
     */
    public int getNbTemplateSwitches() {
        return mNbTemplateSwitches;
    }

    private void bindCarouselItem(PropertyModel model, ViewGroup parent, PropertyKey propertyKey) {
        NoteTemplate template = model.get(NoteProperties.TEMPLATE);

        View carouselItemView = parent.findViewById(R.id.item);
        template.mainBackground.apply(carouselItemView, getNoteCornerRadius());
        carouselItemView.setClipToOutline(true);
        carouselItemView.setContentDescription(
                getActivity().getString(R.string.content_creation_note_template_selected,
                        model.get(NoteProperties.TEMPLATE).localizedName));
        Typeface typeface = model.get(NoteProperties.TYPEFACE);
        LineLimitedTextView noteText = (LineLimitedTextView) parent.findViewById(R.id.text);
        noteText.setTypeface(typeface);
        template.textStyle.apply(noteText, mSelectedText);
        noteText.setIsEllipsizedListener(this::maybeShowToast);

        if (template.contentBackground != null) {
            template.contentBackground.apply(noteText, getNoteCornerRadius());
        } else {
            // Clear the content background.
            noteText.setBackground(null);
        }

        TextView footerLink = (TextView) parent.findViewById(R.id.footer_link);
        TextView footerTitle = (TextView) parent.findViewById(R.id.footer_title);
        ImageView footerIcon = (ImageView) parent.findViewById(R.id.footer_icon);
        footerLink.setText(mUrlDomain);
        footerTitle.setText(mTitle);

        template.footerStyle.apply(footerLink, footerTitle, footerIcon);

        setPadding(model.get(NoteProperties.IS_FIRST), model.get(NoteProperties.IS_LAST),
                parent.findViewById(R.id.item));
    }

    // Adjust the padding for carousel items so that:
    // - When the first item is selected, it is centered and the following item is slightly peaking
    // from the right.
    // - When the last item is selected, it is cenetered and the previous item is
    // slightly peaking from the left.
    // For that, set left padding exactly what is needed to push the first item to the center, but
    // set a smaller padding for the following items (except the last item which has more padding on
    // the right).
    //
    // NOTE: When reading template and padding dimentions don't use |getDimensionPixelSize| as it
    // rounds up the pixel size which brings the sum of template width and right/left paddings 1px
    // larger than the screensize. Because of this the scrolling doesn't work as expected and the
    // selected template doesn't update correctly. See crbug.com/1240537.
    private void setPadding(boolean isFirst, boolean isLast, View itemView) {
        int dialogWidth = getActivity().getResources().getDisplayMetrics().widthPixels;
        int templateWidth = (int) getActivity().getResources().getDimension(R.dimen.note_width);
        int defaultPadding =
                (int) getActivity().getResources().getDimension(R.dimen.note_side_padding);
        int paddingLeft = defaultPadding;
        if (isFirst) {
            paddingLeft =
                    (int) ((dialogWidth - templateWidth) * EXTREMITY_NOTE_PADDING_RATIO + 0.5f);
        }

        int paddingRight = defaultPadding;
        if (isLast) {
            paddingRight =
                    (int) ((dialogWidth - templateWidth) * EXTREMITY_NOTE_PADDING_RATIO + 0.5f);
        }

        MarginLayoutParams params = (MarginLayoutParams) itemView.getLayoutParams();
        params.setMarginStart(paddingLeft);
        params.setMarginEnd(paddingRight);
        itemView.setLayoutParams(params);
        itemView.requestLayout();
    }

    private void maybeShowToast() {
        if (mToast != null) return;

        String toastMessage =
                getActivity().getString(R.string.content_creation_note_shortened_message);
        mToast = Toast.makeText(getActivity(), toastMessage, Toast.LENGTH_LONG);
        mToast.show();
    }

    private float getNoteCornerRadius() {
        return getActivity().getResources().getDimensionPixelSize(R.dimen.note_corner_radius);
    }

    private void focus(int index) {
        ++mNbTemplateSwitches;
        View noteView = getNoteViewAt(index);

        // When scrolling fast the view might be already recycled. See crbug.com/1238306
        if (noteView == null) return;

        noteView.setElevation(
                getActivity().getResources().getDimension(R.dimen.focused_note_elevation));
    }

    private void unFocus(int index) {
        View noteView = getNoteViewAt(index);

        // When scrolling fast the view might be already recycled. See crbug.com/1238306
        if (noteView == null) return;

        noteView.setElevation(0);
    }

    private void centerCurrentNote() {
        RecyclerView noteCarousel = mContentView.findViewById(R.id.note_carousel);
        LinearLayoutManager layoutManager = (LinearLayoutManager) noteCarousel.getLayoutManager();
        int centerOfScreen = getActivity().getResources().getDisplayMetrics().widthPixels / 2
                - getNoteViewAt(mSelectedItemIndex).getWidth() / 2
                - getActivity().getResources().getDimensionPixelSize(R.dimen.note_side_padding);
        layoutManager.scrollToPositionWithOffset(mSelectedItemIndex, centerOfScreen);
    }
}