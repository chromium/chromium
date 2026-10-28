// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks;

import android.app.Activity;
import android.content.Context;
import android.view.LayoutInflater;
import android.view.MenuItem;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewGroup.MarginLayoutParams;

import androidx.appcompat.widget.Toolbar;
import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.RecyclerView;

import org.chromium.base.supplier.NonNullObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.bookmarks.BookmarkListEntry.ViewType;
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
@NullMarked
public class BookmarkFolderPickerCoordinator implements BackPressHandler {
    private final ModelList mModelList = new ModelList();
    private final Context mContext;
    private final BookmarkModel mBookmarkModel;
    private final View mView;
    private final RecyclerView mRecyclerView;
    private final BookmarkFolderPickerMediator mMediator;
    private final boolean mIsDesktopDialog;

    private final SimpleRecyclerViewAdapter mAdapter = new SimpleRecyclerViewAdapter(mModelList);

    /**
     * @param isDesktopDialog Whether the hosting activity uses the desktop dialog rather than the
     *     full-screen presentation.
     * @param restoredFolderId The folder to show instead of the default one, e.g. after the
     *     activity was recreated. Ignored if it no longer exists.
     */
    public BookmarkFolderPickerCoordinator(
            Context context,
            BookmarkModel bookmarkModel,
            List<BookmarkId> bookmarkIds,
            Runnable finishRunnable,
            BookmarkAddNewFolderCoordinator addNewFolderCoordinator,
            BookmarkUiPrefs bookmarkUiPrefs,
            ImprovedBookmarkRowCoordinator improvedBookmarkRowCoordinator,
            ShoppingService shoppingService,
            boolean isFromBookmarkDialog,
            boolean isDesktopDialog,
            @Nullable BookmarkId restoredFolderId) {
        mContext = context;
        mBookmarkModel = bookmarkModel;
        mIsDesktopDialog = isDesktopDialog;
        int layoutId =
                mIsDesktopDialog
                        ? R.layout.bookmark_folder_picker_desktop
                        : R.layout.bookmark_folder_picker;
        mView = LayoutInflater.from(mContext).inflate(layoutId, null);

        mRecyclerView = mView.findViewById(R.id.folder_recycler_view);
        mRecyclerView.setLayoutManager(
                new LinearLayoutManager(context, LinearLayoutManager.VERTICAL, false));
        mRecyclerView.setAdapter(mAdapter);
        mAdapter.registerType(
                ViewType.IMPROVED_BOOKMARK_VISUAL,
                ImprovedBookmarkRow::buildVisualRow,
                ImprovedBookmarkRowViewBinder::bind);
        mAdapter.registerType(
                ViewType.IMPROVED_BOOKMARK_COMPACT,
                this::buildCompactRow,
                ImprovedBookmarkRowViewBinder::bind);
        mAdapter.registerType(
                ViewType.SECTION_HEADER,
                this::buildSectionHeaderView,
                BookmarkManagerViewBinder::bindSectionHeaderView);

        PropertyModel model = new PropertyModel(BookmarkFolderPickerProperties.ALL_KEYS);
        PropertyModelChangeProcessor.create(
                model,
                mView,
                (m, v, k) -> BookmarkFolderPickerViewBinder.bind(m, v, k, mIsDesktopDialog));

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
                        shoppingService,
                        isFromBookmarkDialog,
                        mIsDesktopDialog,
                        restoredFolderId);

        if (mIsDesktopDialog) {
            View backButton = mView.findViewById(R.id.back_button);
            if (backButton != null) {
                backButton.setOnClickListener(
                        (v) -> {
                            if (!mMediator.onBackPressed()) {
                                if (mContext instanceof Activity activity) {
                                    activity.finish();
                                    activity.overridePendingTransition(0, 0);
                                } else {
                                    finishRunnable.run();
                                }
                            }
                        });
            }
        } else {
            FadingShadowView shadow = mView.findViewById(R.id.shadow);
            if (shadow != null) {
                shadow.init(
                        mContext.getColor(R.color.toolbar_shadow_color), FadingShadow.POSITION_TOP);
                mRecyclerView.setOnScrollListener(
                        new RecyclerView.OnScrollListener() {
                            @Override
                            public void onScrolled(RecyclerView recyclerView, int dx, int dy) {
                                super.onScrolled(recyclerView, dx, dy);
                                shadow.setVisibility(
                                        mRecyclerView.canScrollVertically(-1)
                                                ? View.VISIBLE
                                                : View.GONE);
                            }
                        });
            }
        }
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
    public @Nullable Toolbar getToolbar() {
        return mView.findViewById(R.id.toolbar);
    }

    public void updateToolbarButtons() {
        mMediator.updateToolbarButtons();
    }

    public void updateNavigationIcon() {
        mMediator.updateNavigationIconForCurrentParent();
    }

    /** Returns the folder being shown, or null if the model hasn't loaded yet. */
    public @Nullable BookmarkId getCurrentFolderId() {
        return mMediator.getCurrentFolderId();
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

    View buildCompactRow(ViewGroup parent) {
        View row = ImprovedBookmarkRow.buildCompactRow(parent);
        if (mIsDesktopDialog) {
            View container = row.findViewById(R.id.container);
            if (container != null
                    && container.getLayoutParams() instanceof MarginLayoutParams marginParams) {
                marginParams.setMarginStart(0);
                marginParams.setMarginEnd(0);
                container.setLayoutParams(marginParams);
            }
        }
        return row;
    }

    View buildSectionHeaderView(ViewGroup parent) {
        int layoutId =
                mBookmarkModel.areAccountBookmarkFoldersActive()
                        ? R.layout.bookmark_section_header_v2
                        : R.layout.bookmark_section_header;
        View view = LayoutInflater.from(parent.getContext()).inflate(layoutId, parent, false);
        if (mIsDesktopDialog) {
            view.setPaddingRelative(0, view.getPaddingTop(), 0, view.getPaddingBottom());
            if (view.getLayoutParams() instanceof MarginLayoutParams marginParams) {
                marginParams.topMargin = 0;
                view.setLayoutParams(marginParams);
            }
        }
        return view;
    }

    // BackPressHandler implementation.

    @Override
    public @BackPressResult int handleBackPress() {
        return onBackPressed() ? BackPressResult.SUCCESS : BackPressResult.FAILURE;
    }

    @Override
    public NonNullObservableSupplier<Boolean> getHandleBackPressChangedSupplier() {
        return mMediator.getHandleBackPressChangedSupplier();
    }
}
