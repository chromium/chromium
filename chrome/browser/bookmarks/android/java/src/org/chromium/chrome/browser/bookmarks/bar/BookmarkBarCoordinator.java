// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks.bar;

import static android.view.View.VISIBLE;

import static org.chromium.ui.accessibility.KeyboardFocusUtil.setFocus;
import static org.chromium.ui.accessibility.KeyboardFocusUtil.setFocusOnFirstFocusableDescendant;

import android.app.Activity;
import android.util.Pair;
import android.util.TypedValue;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewStub;
import android.widget.ImageView;

import androidx.annotation.NonNull;
import androidx.recyclerview.widget.DefaultItemAnimator;
import androidx.recyclerview.widget.RecyclerView;

import org.chromium.base.Callback;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.Supplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.cc.input.OffsetTag;
import org.chromium.chrome.browser.bookmarks.BookmarkManagerOpener;
import org.chromium.chrome.browser.bookmarks.BookmarkOpener;
import org.chromium.chrome.browser.bookmarks.R;
import org.chromium.chrome.browser.bookmarks.bar.BookmarkBarVisibilityProvider.BookmarkBarVisibilityObserver;
import org.chromium.chrome.browser.browser_controls.BrowserControlsOffsetTagsInfo;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider.ControlsPosition;
import org.chromium.chrome.browser.browser_controls.TopControlLayer;
import org.chromium.chrome.browser.browser_controls.TopControlsStacker;
import org.chromium.chrome.browser.browser_controls.TopControlsStacker.TopControlType;
import org.chromium.chrome.browser.browser_controls.TopControlsStacker.TopControlVisibility;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.layouts.CompositorModelChangeProcessor;
import org.chromium.chrome.browser.layouts.LayoutManager;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.browser_ui.widget.ViewResourceFrameLayout;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;
import org.chromium.ui.modelutil.SimpleRecyclerViewAdapter;
import org.chromium.ui.resources.ResourceManager;
import org.chromium.ui.resources.dynamics.ViewResourceAdapter;

/** Coordinator for the bookmark bar which provides users with bookmark access from top chrome. */
@NullMarked
public class BookmarkBarCoordinator
        implements TopControlLayer,
                BookmarkBarVisibilityObserver,
                View.OnLayoutChangeListener,
                BrowserControlsStateProvider.Observer {

    private final SimpleRecyclerViewAdapter mItemsAdapter;
    private final BookmarkBarItemsLayoutManager mBookmarkBarItemsLayoutManager;
    private final BookmarkBarMediator mMediator;
    private final BookmarkBar mView;
    private final TopControlsStacker mTopControlsStacker;
    private final Callback<@Nullable Void> mHeightChangeCallback;
    private final Runnable mRequestUpdate;
    private final BrowserControlsStateProvider mBrowserControlsStateProvider;
    private final ViewResourceFrameLayout mViewResourceFrameLayout;
    private final ViewResourceAdapter mViewResourceAdapter;
    private final ResourceManager mResourceManager;
    private final BookmarkBarSceneLayer mBookmarkBarSceneLayer;
    private final PropertyModel mBookmarkBarSceneLayerModel;
    private final CompositorModelChangeProcessor mChangeProcessor;
    private boolean mIsResourceRegistered;
    private @Nullable OffsetTag mOffsetTag;

    // Tracks whether or not the bookmark bar should be shown at all. We keep this state in addition
    // to setting visibility directly on |mView| because we need to differentiate the Android
    // widgets from the bookmark bar in general.
    private boolean mShouldBookmarkBarBeShown;

    /**
     * Constructs the bookmark bar coordinator.
     *
     * @param activity The activity which is hosting the bookmark bar.
     * @param layoutManager LayoutManager to add SceneLayer to and bind model to.
     * @param requestUpdate Runnable to request an update for the layout manager and cc layers.
     * @param resourceManager The resource manager for providing resources to C++ layers.
     * @param browserControlsStateProvider The state provider for browser controls.
     * @param heightChangeCallback A callback to notify owner of bookmark bar height changes.
     * @param profileSupplier The supplier for the currently active profile.
     * @param viewStub The stub used to inflate the bookmark bar.
     * @param currentTab The current tab if it exists.
     * @param bookmarkOpener Used to open bookmarks.
     * @param bookmarkManagerOpenerSupplier Used to open the bookmark manager.
     * @param topControlsStacker TopControlsStacker to manage the view's y-offset.
     */
    public BookmarkBarCoordinator(
            Activity activity,
            LayoutManager layoutManager,
            Runnable requestUpdate,
            ResourceManager resourceManager,
            BrowserControlsStateProvider browserControlsStateProvider,
            Callback<@Nullable Void> heightChangeCallback,
            ObservableSupplier<Profile> profileSupplier,
            ViewStub viewStub,
            @Nullable Tab currentTab,
            BookmarkOpener bookmarkOpener,
            ObservableSupplier<BookmarkManagerOpener> bookmarkManagerOpenerSupplier,
            TopControlsStacker topControlsStacker) {
        mRequestUpdate = requestUpdate;
        mResourceManager = resourceManager;

        mView = (BookmarkBar) viewStub.inflate();
        mViewResourceFrameLayout = mView.findViewById(R.id.bookmark_bar_view_resource_frame_layout);
        mViewResourceAdapter = mViewResourceFrameLayout.getResourceAdapter();
        registerResource();

        mBookmarkBarSceneLayer =
                new BookmarkBarSceneLayer(mViewResourceFrameLayout.getId(), mResourceManager);
        mBookmarkBarSceneLayer.setVisibility(true);

        mHeightChangeCallback = heightChangeCallback;
        mView.addOnLayoutChangeListener(this);
        mShouldBookmarkBarBeShown = true;

        mBrowserControlsStateProvider = browserControlsStateProvider;
        mBrowserControlsStateProvider.addObserver(this);

        // Bind view/model for 'All Bookmarks' button.
        final var allBookmarksButtonModel =
                new PropertyModel.Builder(BookmarkBarButtonProperties.ALL_KEYS).build();

        final BookmarkBarButton allBookmarksButton =
                mViewResourceFrameLayout.findViewById(R.id.bookmark_bar_all_bookmarks_button);

        // Binds the model to the view.
        PropertyModelChangeProcessor.create(
                allBookmarksButtonModel, allBookmarksButton, BookmarkBarButtonViewBinder::bind);
        ImageView starIcon = allBookmarksButton.findViewById(R.id.bookmark_bar_button_icon);

        // We need this because otherwise the star icon is lower than the "All Bookmarks" text. If
        // we add setTranslationY directly in bookmark_bar_button_icon in bookmark_bar_button.xml,
        // the top parts of the web page icons in the bookmarks bar are cut off.
        if (starIcon != null) {
            final float translationInDp = -2f;
            // Converts dp values to raw pixels.
            float translationInPx =
                    TypedValue.applyDimension(
                            TypedValue.COMPLEX_UNIT_DIP,
                            translationInDp,
                            activity.getResources().getDisplayMetrics());
            starIcon.setTranslationY(translationInPx);
        }

        // Bind adapter/model and initialize view for bookmark bar items.
        final var itemsModel = new ModelList();
        mItemsAdapter = new SimpleRecyclerViewAdapter(itemsModel);
        mItemsAdapter.registerType(
                BookmarkBarUtils.ViewType.ITEM,
                this::inflateBookmarkBarButton,
                BookmarkBarButtonViewBinder::bind);
        final RecyclerView itemsContainer =
                mViewResourceFrameLayout.findViewById(R.id.bookmark_bar_items_container);
        itemsContainer.setAdapter(mItemsAdapter);
        mBookmarkBarItemsLayoutManager = new BookmarkBarItemsLayoutManager(activity);
        mBookmarkBarItemsLayoutManager.setItemMaxWidth(
                activity.getResources().getDimensionPixelSize(R.dimen.bookmark_bar_item_max_width));
        itemsContainer.setLayoutManager(mBookmarkBarItemsLayoutManager);

        // NOTE: Scrolling isn't supported and items rarely change so item view caching is disabled.
        itemsContainer.getRecycledViewPool().setMaxRecycledViews(BookmarkBarUtils.ViewType.ITEM, 0);
        itemsContainer.setItemViewCacheSize(0);
        itemsContainer.setItemAnimator(
                new BookmarkButtonItemAnimator(this::handleBookmarkBarChange));

        Supplier<Pair<Integer, Integer>> controlsHeightSupplier =
                () ->
                        new Pair<>(
                                mBrowserControlsStateProvider.getTopControlsHeight(),
                                mBrowserControlsStateProvider.getBottomControlsHeight());

        // Bind view/model for bookmark bar and instantiate mediator.
        final var model = new PropertyModel.Builder(BookmarkBarProperties.ALL_KEYS).build();
        mMediator =
                new BookmarkBarMediator(
                        activity,
                        allBookmarksButtonModel,
                        controlsHeightSupplier,
                        itemsModel,
                        mBookmarkBarItemsLayoutManager,
                        model,
                        profileSupplier,
                        currentTab,
                        bookmarkOpener,
                        bookmarkManagerOpenerSupplier,
                        itemsContainer,
                        mView,
                        this::handleBookmarkBarChange);
        PropertyModelChangeProcessor.create(model, mView, BookmarkBarViewBinder::bind);

        // All dimensions and offsets require the first layout pass to complete, so don't set here.
        mBookmarkBarSceneLayerModel =
                new PropertyModel.Builder(BookmarkBarSceneLayerProperties.ALL_KEYS)
                        .with(BookmarkBarSceneLayerProperties.VISIBILITY, true)
                        .build();
        updateOffsetTag();
        mChangeProcessor =
                layoutManager.createCompositorMCP(
                        mBookmarkBarSceneLayerModel,
                        mBookmarkBarSceneLayer,
                        BookmarkBarSceneLayer::bind);

        mTopControlsStacker = topControlsStacker;
        mTopControlsStacker.addControl(this);
    }

    /** Destroys the bookmark bar coordinator. */
    public void destroy() {
        mTopControlsStacker.removeControl(this);
        mItemsAdapter.destroy();
        mMediator.destroy();
        mChangeProcessor.destroy();
        mView.removeOnLayoutChangeListener(this);
        mBrowserControlsStateProvider.removeObserver(this);
        if (mIsResourceRegistered) unregisterResource();
        mBookmarkBarSceneLayer.setVisibility(false);
        handleBookmarkBarChange();
    }

    private void registerResource() {
        if (mIsResourceRegistered) return;
        mResourceManager
                .getBitmapDynamicResourceLoader()
                .registerResource(mViewResourceFrameLayout.getId(), mViewResourceAdapter);
        mIsResourceRegistered = true;
    }

    private void unregisterResource() {
        if (!mIsResourceRegistered) return;
        mViewResourceAdapter.dropCachedBitmap();
        mResourceManager
                .getBitmapDynamicResourceLoader()
                .unregisterResource(mViewResourceFrameLayout.getId());
        mIsResourceRegistered = false;
    }

    /**
     * Handles changes to the bookmarks bar that require a new snapshot for the scene layer and an
     * invalidation of the layout. This method should be called after the visible view to the user
     * has changed, such as after bookmarks are added/removed or reordered.
     */
    public void handleBookmarkBarChange() {
        mViewResourceAdapter.triggerBitmapCapture();
        mViewResourceAdapter.invalidate(null);
        mRequestUpdate.run();
        mBookmarkBarSceneLayer.updateProperties(mBookmarkBarSceneLayerModel);
    }

    /**
     * @return Provides the scene layer that backs the snapshotting logic for the Bookmark Bar.
     */
    public BookmarkBarSceneLayer getSceneLayer() {
        return mBookmarkBarSceneLayer;
    }

    /**
     * @return Whether the Bookmark Bar is current visible to the user.
     */
    public boolean isVisible() {
        return mView != null && mView.getVisibility() == VISIBLE;
    }

    /**
     * Sets whether the Bookmark Bar should be visible to the user. This will
     * unregister/(re-)register the ViewResourceFrameLayout with the ResourceManager, but will not
     * destroy any underlying objects.
     *
     * @param isVisible Whether or not the Bookmark Bar is visible to the user.
     */
    public void setVisibility(boolean isVisible) {
        mShouldBookmarkBarBeShown = isVisible;
        mMediator.setVisibility(isVisible);
        mBookmarkBarSceneLayer.setVisibility(isVisible);
        if (!isVisible) {
            unregisterResource();
        } else {
            registerResource();
            handleBookmarkBarChange();
        }
    }

    /** Requests focus within the bookmark bar. */
    public void requestFocus() {
        if (setFocusOnFirstFocusableDescendant(
                mViewResourceFrameLayout.findViewById(R.id.bookmark_bar_items_container))) {
            // If we set focus on a bookmark in the RecyclerView of user bookmarks, we are done.
            return;
        }
        // Otherwise (there were no user bookmarks), focus on the all bookmarks button at the end.
        setFocus(mViewResourceFrameLayout.findViewById(R.id.bookmark_bar_all_bookmarks_button));
    }

    /**
     * @return Whether keyboard focus is within this view.
     */
    public boolean hasKeyboardFocus() {
        return mView.getFocusedChild() != null;
    }

    // TopControlLayer implementation:

    @Override
    public @TopControlType int getTopControlType() {
        return TopControlType.BOOKMARK_BAR;
    }

    @Override
    public int getTopControlHeight() {
        return mShouldBookmarkBarBeShown ? mView.getHeight() : 0;
    }

    @Override
    public int getTopControlVisibility() {
        // This could always return TopControlVisibility.VISIBLE assuming that on all visibility
        // changes {@link TabbedRootUiCoordinator#destroyBookmarkBarIfNecessary} is called,
        // but for correctness we will check view visibility just in case.
        // TODO(crbug.com/417238089): Possibly add way to notify stacker of visibility changes.
        return mView != null && mView.getVisibility() == VISIBLE
                ? TopControlVisibility.VISIBLE
                : TopControlVisibility.HIDDEN;
    }

    @Override
    public void onTopControlLayerHeightChanged(int topControlsHeight, int topControlsMinHeight) {
        assert ChromeFeatureList.sTopControlsRefactor.isEnabled()
                : "onTopControlLayerHeightChanged should not be called unless refactor is enabled";

        // Here we are subtracting the height of the TopControl, |mView|, to bottom align the
        // BookmarkBar relative to the other TopControls.
        // TODO(crbug.com/417238089): We should not hardcode this offset functionality since it
        // assumes an absolute BookmarkBar position, and fails when topControlsHeight becomes 0.
        mMediator.setTopMargin(topControlsHeight - getTopControlHeight());
    }

    // BookmarkBarVisibilityObserver implementation:

    @Override
    public void onMaxWidthChanged(int maxWidth) {
        mBookmarkBarItemsLayoutManager.setItemMaxWidth(maxWidth);
    }

    // View.OnLayoutChangeListener implementation:

    @Override
    public void onLayoutChange(
            View v,
            int left,
            int top,
            int right,
            int bottom,
            int oldLeft,
            int oldTop,
            int oldRight,
            int oldBottom) {
        // This layout change listener is used on |mView|, which is the entire Bookmarks Bar. The
        // View's width/height are thus the width/height of the entire scene layer, and its padding
        // defines the offset of the snapshot within the SceneLayer of the tightly bound
        // |mViewResourceFrameLayout|.
        final int oldHeight = oldBottom - oldTop;
        final int newHeight = bottom - top;
        if (newHeight != oldHeight) {
            mBookmarkBarSceneLayerModel.set(
                    BookmarkBarSceneLayerProperties.SCENE_LAYER_HEIGHT, newHeight);
            mHeightChangeCallback.onResult(null);
        }
        final int oldWidth = oldRight - oldLeft;
        final int newWidth = right - left;
        if (oldWidth != newWidth) {
            mBookmarkBarSceneLayerModel.set(
                    BookmarkBarSceneLayerProperties.SCENE_LAYER_WIDTH, newWidth);
            handleBookmarkBarChange();
        }
        mBookmarkBarSceneLayerModel.set(
                BookmarkBarSceneLayerProperties.SNAPSHOT_OFFSET_WIDTH, v.getPaddingLeft());
        mBookmarkBarSceneLayerModel.set(
                BookmarkBarSceneLayerProperties.SNAPSHOT_OFFSET_HEIGHT, v.getPaddingTop());
    }

    // BrowserControlsStateProvider.Observer implementation:

    @Override
    public void onControlsOffsetChanged(
            int topOffset,
            int topControlsMinHeightOffset,
            boolean topControlsMinHeightChanged,
            int bottomOffset,
            int bottomControlsMinHeightOffset,
            boolean bottomControlsMinHeightChanged,
            boolean requestNewFrame,
            boolean isVisibilityForced) {
        // When the top controls offset has changed to a non-zero value, it means that the top
        // controls are scrolling offscreen (or still coming back onscreen). When in this state,
        // we want to hide the Android widgets (which are controlled by the Mediator). We do not
        // also set the sceneLayer visibility here because we want that to be what is shown.
        mMediator.setVisibility(
                mShouldBookmarkBarBeShown
                        && mBrowserControlsStateProvider.getTopControlOffset() == 0);
        mMediator.onBrowserControlsChanged(
                mBrowserControlsStateProvider.getTopControlsHeight(),
                mBrowserControlsStateProvider.getBottomControlsHeight());
        mMediator.setTopMargin(sceneLayerHeightOffset());
    }

    @Override
    public void onTopControlsHeightChanged(int topControlsHeight, int topControlsMinHeight) {
        // TODO(crbug.com/430058918): Replace w/ positioning construct like `BottomControlsStacker`.
        mMediator.setTopMargin(sceneLayerHeightOffset());
        mMediator.onBrowserControlsChanged(
                topControlsHeight, mBrowserControlsStateProvider.getBottomControlsHeight());
        mBookmarkBarSceneLayerModel.set(
                BookmarkBarSceneLayerProperties.SCENE_LAYER_OFFSET_HEIGHT,
                sceneLayerHeightOffset());
    }

    @Override
    public void onControlsConstraintsChanged(
            BrowserControlsOffsetTagsInfo oldOffsetTagsInfo,
            BrowserControlsOffsetTagsInfo offsetTagsInfo,
            int constraints,
            boolean shouldUpdateOffsets) {
        if (ChromeFeatureList.sBrowserControlsInViz.isEnabled()) {
            mOffsetTag = offsetTagsInfo.getTopControlsOffsetTag();
            updateOffsetTag();
        }
    }

    @Override
    public void onControlsPositionChanged(
            @BrowserControlsStateProvider.ControlsPosition int controlsPosition) {
        if (ChromeFeatureList.sBrowserControlsInViz.isEnabled()) {
            updateOffsetTag();
        }
    }

    // Private methods:

    private BookmarkBarButton inflateBookmarkBarButton(ViewGroup parent) {
        return (BookmarkBarButton)
                LayoutInflater.from(parent.getContext())
                        .inflate(R.layout.bookmark_bar_button, parent, false);
    }

    private int sceneLayerHeightOffset() {
        // Top controls height is the sum of all top browser control heights which includes that of
        // the bookmark bar. Subtract the bookmark bar's height from the top controls height when
        // calculating offset/topMargin in order to bottom align the bookmark bar relative to other
        // top browser controls.
        return mBrowserControlsStateProvider.getTopControlsHeight() - getTopControlHeight();
    }

    private void updateOffsetTag() {
        // The Bookmarks Bar will only be present when the control container is at the top.
        if (mBrowserControlsStateProvider.getControlsPosition() == ControlsPosition.TOP) {
            mBookmarkBarSceneLayerModel.set(BookmarkBarSceneLayerProperties.OFFSET_TAG, mOffsetTag);
        } else {
            mBookmarkBarSceneLayerModel.set(BookmarkBarSceneLayerProperties.OFFSET_TAG, null);
        }
    }

    // Custom animator for BookmarkBar RecyclerView:

    /**
     * Custom ItemAnimator for the Bookmark Bar. We take snapshots on changes in the bookmark bar to
     * use in C++-side layers. However, if we do a snapshot when an item is added or moved, the
     * short animation of the RecyclerView can result in a snapshot of the bookmark bar in a
     * transient state. Instead we add a custom animator to the RecyclerView to trigger a snapshot
     * at the end of any animation, which includes adding, deleting, or re-ordering any bookmarks.
     */
    private static class BookmarkButtonItemAnimator extends DefaultItemAnimator {
        private final Runnable mPostAnimationRunnable;

        public BookmarkButtonItemAnimator(Runnable mPostAnimationRunnable) {
            this.mPostAnimationRunnable = mPostAnimationRunnable;
        }

        @Override
        public void onAnimationFinished(@NonNull RecyclerView.ViewHolder viewHolder) {
            super.onAnimationFinished(viewHolder);
            mPostAnimationRunnable.run();
        }
    }
}
