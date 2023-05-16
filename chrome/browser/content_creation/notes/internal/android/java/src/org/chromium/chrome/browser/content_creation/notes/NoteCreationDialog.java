// Copyright 2021 The Chromium Authors
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
import android.view.accessibility.AccessibilityEvent;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.RelativeLayout;
import android.widget.ScrollView;
import android.widget.TextView;

import androidx.appcompat.app.AlertDialog;
import androidx.fragment.app.DialogFragment;
import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.PagerSnapHelper;
import androidx.recyclerview.widget.RecyclerView;
import androidx.recyclerview.widget.RecyclerView.OnScrollListener;
import androidx.recyclerview.widget.SnapHelper;

import org.chromium.chrome.browser.content_creation.internal.R;
import org.chromium.components.browser_ui.widget.FullscreenAlertDialog;
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
    private int mNbTemplateSwitches;
    private boolean mInitialized;
    private Runnable mExecuteActionForAccessibility;

    interface NoteDialogObserver {
        void onViewCreated(View view);
    }
    private NoteDialogObserver mNoteDialogObserver;

    public void initDialog(NoteDialogObserver noteDialogObserver, String urlDomain, String title,
            String selectedText, Runnable executeActionForAccessibility) {
        mNoteDialogObserver = noteDialogObserver;
        mUrlDomain = urlDomain;
        mTitle = title;
        mSelectedText = selectedText;
        mInitialized = true;
        mExecuteActionForAccessibility = executeActionForAccessibility;
    }

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        // Don't create dialog before it is initialized.
        if (!mInitialized) dismiss();
    }

    @Override
    public Dialog onCreateDialog(Bundle savedInstanceState) {
        AlertDialog.Builder builder = new FullscreenAlertDialog.Builder(getActivity());
        mContentView = getActivity().getLayoutInflater().inflate(R.layout.creation_dialog, null);
        builder.setView(mContentView);

        setTopMargin();
        addOrRemoveScrollView();

        if (mNoteDialogObserver != null) mNoteDialogObserver.onViewCreated(mContentView);

        return builder.create();
    }

    @Override
    public void onConfigurationChanged(Configuration newConfig) {
        super.onConfigurationChanged(newConfig);

        // There is a corner case where this function can be triggered by toggling the battery saver
        // state, resulting in all the variables being reset. The only way out is to destroy this
        // dialog to bring the user back to the web page.
        if (!mInitialized) {
            dismiss();
            return;
        }

        // Title top margin depends on screen orientation.
        setTopMargin();

        // Add or remove scroll view as needed.
        addOrRemoveScrollView();

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

        SimpleRecyclerViewAdapter adapter = new SimpleRecyclerViewAdapter(carouselItems) {
            @Override
            public void onBindViewHolder(
                    SimpleRecyclerViewAdapter.ViewHolder holder, int position) {
                holder.itemView.setTag(position);
                super.onBindViewHolder(holder, position);
            }
        };
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

                int newSelectedItemIndex = (last_visible - first_visible) / 2 + first_visible;
                if (mSelectedItemIndex == newSelectedItemIndex) {
                    return;
                }
                unFocus(mSelectedItemIndex);
                mSelectedItemIndex = newSelectedItemIndex;
                setSelectedItemTitle(carouselItems.get(mSelectedItemIndex).model);
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
        // If we are creating the first card and it is the selected one, we should update the title.
        if (model.get(NoteProperties.IS_FIRST) && mSelectedItemIndex == 0) {
            setSelectedItemTitle(model);
        }

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

        carouselItemView.setAccessibilityDelegate(new View.AccessibilityDelegate() {
            @Override
            public void onPopulateAccessibilityEvent(View host, AccessibilityEvent event) {
                int position;
                switch (event.getEventType()) {
                    case AccessibilityEvent.TYPE_VIEW_ACCESSIBILITY_FOCUSED:
                        host.setClickable(true);
                        mSelectedItemIndex = (Integer) host.getTag();
                        centerCurrentNote();
                        break;
                    case AccessibilityEvent.TYPE_VIEW_CLICKED:
                        mExecuteActionForAccessibility.run();
                        break;
                }

                super.onPopulateAccessibilityEvent(host, event);
            }
        });
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

    private void setTopMargin() {
        // Push down the note title depending on screensize.
        int minTopMargin = getActivity().getResources().getDimensionPixelSize(
                R.dimen.note_title_min_top_margin);
        int screenHeight = getActivity().getResources().getDisplayMetrics().heightPixels;
        int topMarginOffset = getActivity().getResources().getDimensionPixelSize(
                R.dimen.note_title_top_margin_offset);
        int templateWidth =
                (int) getActivity().getResources().getDimensionPixelSize(R.dimen.note_width);

        View firstView = mContentView.findViewById(R.id.title);
        MarginLayoutParams params = (MarginLayoutParams) firstView.getLayoutParams();
        params.topMargin = (int) (minTopMargin + (screenHeight - topMarginOffset) * 0.15f);
        firstView.setLayoutParams(params);
    }

    private void addScrollView() {
        assert mContentView.findViewById(R.id.scrollview) == null;

        ScrollView scrollView = new ScrollView(getActivity());
        scrollView.setLayoutParams(new LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.MATCH_PARENT));
        scrollView.setId(R.id.scrollview);

        LinearLayout dialogLayout = mContentView.findViewById(R.id.dialog_layout);
        RelativeLayout mainContent = dialogLayout.findViewById(R.id.main_content);
        dialogLayout.removeView(mainContent);
        scrollView.addView(mainContent);
        dialogLayout.addView(scrollView);
    }

    private void removeScrollView() {
        assert mContentView.findViewById(R.id.scrollview) != null;

        LinearLayout dialogLayout = mContentView.findViewById(R.id.dialog_layout);
        RelativeLayout mainContent = dialogLayout.findViewById(R.id.main_content);
        ScrollView scrollView = dialogLayout.findViewById(R.id.scrollview);
        scrollView.removeView(mainContent);
        dialogLayout.removeView(scrollView);
        dialogLayout.addView(mainContent);
    }

    private void addOrRemoveScrollView() {
        int screenHeight = getActivity().getResources().getDisplayMetrics().heightPixels;
        int dialogHeight =
                (int) getActivity().getResources().getDimension(R.dimen.min_dialog_height);
        if (dialogHeight < screenHeight && mContentView.findViewById(R.id.scrollview) != null) {
            removeScrollView();
        }

        if (dialogHeight > screenHeight && mContentView.findViewById(R.id.scrollview) == null) {
            addScrollView();
        }
    }

    private void setSelectedItemTitle(PropertyModel model) {
        assert mContentView != null;
        ((TextView) mContentView.findViewById(R.id.title))
                .setText(model.get(NoteProperties.TEMPLATE).localizedName);
    }
}