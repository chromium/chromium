// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.MenuItem;
import android.view.View;
import android.view.ViewGroup;

import androidx.appcompat.widget.Toolbar;
import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.RecyclerView;

import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.bookmarks.BookmarkListEntry.ViewType;
import org.chromium.chrome.browser.bookmarks.BookmarkUiPrefs.BookmarkRowDisplayPref;
import org.chromium.components.bookmarks.BookmarkId;
import org.chromium.components.browser_ui.widget.FadingShadow;
import org.chromium.components.browser_ui.widget.FadingShadowView;
import org.chromium.components.browser_ui.widget.gesture.BackPressHandler;
import org.chromium.components.commerce.core.ShoppingService;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;
import org.chromium.ui.modelutil.SimpleRecyclerViewAdapter;

import java.util.List;

/** Coordinates the views/mediators that make up the bookmark folder picker. */
public class BookmarkFolderPickerCoordinator implements BackPressHandler {
    private final ObservableSupplierImpl<Boolean> mBackPressStateSupplier =
            new ObservableSupplierImpl<>();
    private final ModelList mModelList = new ModelList();
    private final Context mContext;
    private final BookmarkModel mBookmarkModel;
    private final View mView;
    private final View mMoveButton;
    private final RecyclerView mRecyclerView;
    private final BookmarkFolderPickerMediator mMediator;
    private final BookmarkUiPrefs mBookmarkUiPrefs;

    private final SimpleRecyclerViewAdapter mAdapter = new SimpleRecyclerViewAdapter(mModelList);

    public BookmarkFolderPickerCoordinator(
            Context context,
            BookmarkModel bookmarkModel,
            List<BookmarkId> bookmarkIds,
            Runnable finishRunnable,
            BookmarkAddNewFolderCoordinator addNewFolderCoordinator,
            BookmarkUiPrefs bookmarkUiPrefs,
            ImprovedBookmarkRowCoordinator improvedBookmarkRowCoordinator,
            ShoppingService shoppingService) {
        mContext = context;
        mBookmarkModel = bookmarkModel;
        mView = LayoutInflater.from(mContext).inflate(R.layout.bookmark_folder_picker, null);
        mMoveButton = mView.findViewById(R.id.move_button);

        mRecyclerView = mView.findViewById(R.id.folder_recycler_view);
        mRecyclerView.setLayoutManager(
                new LinearLayoutManager(context, LinearLayoutManager.VERTICAL, false));
        mRecyclerView.setAdapter(mAdapter);
        mAdapter.registerType(
                ViewType.IMPROVED_BOOKMARK_VISUAL,
                BookmarkManagerCoordinator::buildVisualImprovedBookmarkRow,
                ImprovedBookmarkRowViewBinder::bind);
        mAdapter.registerType(
                ViewType.IMPROVED_BOOKMARK_COMPACT,
                BookmarkManagerCoordinator::buildCompactImprovedBookmarkRow,
                ImprovedBookmarkRowViewBinder::bind);
        mAdapter.registerType(
                ViewType.SECTION_HEADER,
                this::buildSectionHeaderView,
                BookmarkManagerViewBinder::bindSectionHeaderView);

        PropertyModel model = new PropertyModel(BookmarkFolderPickerProperties.ALL_KEYS);
        PropertyModelChangeProcessor.create(model, mView, BookmarkFolderPickerViewBinder::bind);

        mMediator =
                new BookmarkFolderPickerMediator(
                        context,
                        bookmarkModel,
                        bookmarkIds,
                        finishRunnable,
                        bookmarkUiPrefs,
                        model,
                        mModelList,
                        addNewFolderCoordinator,
                        improvedBookmarkRowCoordinator,
                        shoppingService);

        FadingShadowView shadow = mView.findViewById(R.id.shadow);
        shadow.init(mContext.getColor(R.color.toolbar_shadow_color), FadingShadow.POSITION_TOP);
        mRecyclerView.setOnScrollListener(
                new RecyclerView.OnScrollListener() {
                    @Override
                    public void onScrollStateChanged(RecyclerView recyclerView, int newState) {
                        super.onScrollStateChanged(recyclerView, newState);
                    }

                    @Override
                    public void onScrolled(RecyclerView recyclerView, int dx, int dy) {
                        super.onScrolled(recyclerView, dx, dy);
                        shadow.setVisibility(
                                mRecyclerView.canScrollVertically(-1) ? View.VISIBLE : View.GONE);
                    }
                });

        // Back presses are always handled.
        mBackPressStateSupplier.set(true);

        mBookmarkUiPrefs = bookmarkUiPrefs;
    }

    /** Destroys the coordinator. */
    public void destroy() {
        mMediator.destroy();
    }

    /** Returns the view for display. */
    public View getView() {
        return mView;
    }

    /** Returns the {@link Toolbar} for the folder picker. */
    public Toolbar getToolbar() {
        return mView.findViewById(R.id.toolbar);
    }

    public void updateToolbarButtons() {
        mMediator.updateToolbarButtons();
    }

    // Delegate setup methods.

    /** Handle option menu selections. */
    public boolean optionsItemSelected(MenuItem item) {
        return mMediator.optionsItemSelected(item.getItemId());
    }

    public boolean onBackPressed() {
        return mMediator.onBackPressed();
    }

    // Building rows for the recycler view.

    View buildFolderRow(ViewGroup parent) {
        ImprovedBookmarkRow row =
                ImprovedBookmarkRow.buildView(
                        parent.getContext(),
                        mBookmarkUiPrefs.getBookmarkRowDisplayPref()
                                == BookmarkRowDisplayPref.VISUAL);
        return row;
    }

    View buildSectionHeaderView(ViewGroup parent) {
        int layoutId =
                mBookmarkModel.areAccountBookmarkFoldersActive()
                        ? R.layout.bookmark_section_header_v2
                        : R.layout.bookmark_section_header;
        return LayoutInflater.from(parent.getContext()).inflate(layoutId, parent, false);
    }

    // BackPressHandler implementation.

    @Override
    public @BackPressResult int handleBackPress() {
        return onBackPressed() ? BackPressResult.SUCCESS : BackPressResult.FAILURE;
    }

    @Override
    public ObservableSupplier<Boolean> getHandleBackPressChangedSupplier() {
        return mBackPressStateSupplier;
    }

    // Testing methods.

    void openFolderForTesting(BookmarkId folder) {
        mMediator.populateFoldersForParentId(folder);
    }
}
