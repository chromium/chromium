// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.widget.selection;

import android.content.Context;
import android.content.res.Configuration;
import android.content.res.Resources;
import android.support.v4.view.ViewCompat;
import android.support.v7.widget.LinearLayoutManager;
import android.support.v7.widget.RecyclerView;
import android.support.v7.widget.RecyclerView.AdapterDataObserver;
import android.support.v7.widget.RecyclerView.ItemAnimator;
import android.support.v7.widget.RecyclerView.OnScrollListener;
import android.support.v7.widget.Toolbar.OnMenuItemClickListener;
import android.util.AttributeSet;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewStub;
import android.widget.FrameLayout;
import android.widget.TextView;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.gesturenav.HistoryNavigationDelegate;
import org.chromium.chrome.browser.gesturenav.HistoryNavigationLayout;
import org.chromium.chrome.browser.ui.widget.FadingShadow;
import org.chromium.chrome.browser.ui.widget.FadingShadowView;
import org.chromium.chrome.browser.ui.widget.LoadingView;
import org.chromium.chrome.browser.ui.widget.displaystyle.DisplayStyleObserver;
import org.chromium.chrome.browser.ui.widget.displaystyle.HorizontalDisplayStyle;
import org.chromium.chrome.browser.ui.widget.displaystyle.UiConfig;
import org.chromium.chrome.browser.ui.widget.displaystyle.UiConfig.DisplayStyle;
import org.chromium.chrome.browser.widget.selection.SelectionDelegate.SelectionObserver;

import java.util.List;

/**
 * Contains UI elements common to selectable list views: a loading view, empty view, selection
 * toolbar, shadow, and RecyclerView.
 *
 * After the SelectableListLayout is inflated, it should be initialized through calls to
 * #initializeRecyclerView(), #initializeToolbar(), and #initializeEmptyView().
 *
 * @param <E> The type of the selectable items this layout holds.
 */
public class SelectableListLayout<E>
        extends FrameLayout implements DisplayStyleObserver, SelectionObserver<E> {

    private static final int WIDE_DISPLAY_MIN_PADDING_DP = 16;
    private RecyclerView.Adapter mAdapter;
    private ViewStub mToolbarStub;
    private TextView mEmptyView;
    private View mEmptyViewWrapper;
    private LoadingView mLoadingView;
    private RecyclerView mRecyclerView;
    private ItemAnimator mItemAnimator;
    SelectableListToolbar<E> mToolbar;
    private FadingShadowView mToolbarShadow;
    boolean mShowShadowOnSelection;

    private int mEmptyStringResId;
    private int mSearchEmptyStringResId;

    private UiConfig mUiConfig;

    private final AdapterDataObserver mAdapterObserver = new AdapterDataObserver() {
        @Override
        public void onChanged() {
            super.onChanged();
            updateLayout();
            // At inflation, the RecyclerView is set to gone, and the loading view is visible. As
            // long as the adapter data changes, we show the recycler view, and hide loading view.
            mLoadingView.hideLoadingUI();
        }

        @Override
        public void onItemRangeInserted(int positionStart, int itemCount) {
            super.onItemRangeInserted(positionStart, itemCount);
            updateLayout();
            // At inflation, the RecyclerView is set to gone, and the loading view is visible. As
            // long as the adapter data changes, we show the recycler view, and hide loading view.
            mLoadingView.hideLoadingUI();
        }

        @Override
        public void onItemRangeRemoved(int positionStart, int itemCount) {
            super.onItemRangeRemoved(positionStart, itemCount);
            updateLayout();
        }
    };

    public SelectableListLayout(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();

        LayoutInflater.from(getContext()).inflate(R.layout.selectable_list_layout, this);

        mEmptyView = (TextView) findViewById(R.id.empty_view);
        mEmptyViewWrapper = findViewById(R.id.empty_view_wrapper);
        mLoadingView = (LoadingView) findViewById(R.id.loading_view);
        mLoadingView.showLoadingUI();

        mToolbarStub = (ViewStub) findViewById(R.id.action_bar_stub);

        setFocusable(true);
        setFocusableInTouchMode(true);
    }

    @Override
    protected void onConfigurationChanged(Configuration newConfig) {
        super.onConfigurationChanged(newConfig);

        if (mUiConfig != null) mUiConfig.updateDisplayStyle();
    }

    /**
     * Creates a RecyclerView for the given adapter.
     *
     * @param adapter The adapter that provides a binding from an app-specific data set to views
     *                that are displayed within the RecyclerView.
     * @return The RecyclerView itself.
     */
    public RecyclerView initializeRecyclerView(RecyclerView.Adapter adapter) {
        return initializeRecyclerView(adapter, null);
    }

    /**
     * Initializes the layout with the given recycler view and adapter.
     *
     * @param adapter The adapter that provides a binding from an app-specific data set to views
     *                that are displayed within the RecyclerView.
     * @param recyclerView The recycler view to be shown.
     * @return The RecyclerView itself.
     */
    public RecyclerView initializeRecyclerView(
            RecyclerView.Adapter adapter, @Nullable RecyclerView recyclerView) {
        mAdapter = adapter;

        if (recyclerView == null) {
            mRecyclerView = (RecyclerView) findViewById(R.id.recycler_view);
            mRecyclerView.setLayoutManager(new LinearLayoutManager(getContext()));
        } else {
            mRecyclerView = recyclerView;

            // Replace the inflated recycler view with the one supplied to this method.
            FrameLayout contentView = (FrameLayout) findViewById(R.id.list_content);
            RecyclerView existingView = (RecyclerView) contentView.findViewById(R.id.recycler_view);
            contentView.removeView(existingView);
            contentView.addView(mRecyclerView, 0);
        }

        mRecyclerView.setAdapter(mAdapter);
        initializeRecyclerViewProperties();
        return mRecyclerView;
    }

    private void initializeRecyclerViewProperties() {
        mAdapter.registerAdapterDataObserver(mAdapterObserver);

        mRecyclerView.setHasFixedSize(true);
        mRecyclerView.addOnScrollListener(new OnScrollListener() {
            @Override
            public void onScrolled(RecyclerView recyclerView, int dx, int dy) {
                setToolbarShadowVisibility();
            }
        });

        mItemAnimator = mRecyclerView.getItemAnimator();
    }

    /**
     * Initializes the SelectionToolbar.
     *
     * @param toolbarLayoutId The resource id of the toolbar layout. This will be inflated into
     *                        a ViewStub.
     * @param delegate The SelectionDelegate that will inform the toolbar of selection changes.
     * @param titleResId The resource id of the title string. May be 0 if this class shouldn't set
     *                   set a title when the selection is cleared.
     * @param normalGroupResId The resource id of the menu group to show when a selection isn't
     *                         established.
     * @param selectedGroupResId The resource id of the menu item to show when a selection is
     *                           established.
     * @param listener The OnMenuItemClickListener to set on the toolbar.
     * @param showShadowOnSelection Whether to show the toolbar shadow on selection.
     * @param updateStatusBarColor Whether the status bar color should be updated to match the
     *                             toolbar color. If true, the status bar will only be updated if
     *                             the current device fully supports theming and is on Android M+.
     * @return The initialized SelectionToolbar.
     */
    public SelectableListToolbar<E> initializeToolbar(int toolbarLayoutId,
            SelectionDelegate<E> delegate, int titleResId, int normalGroupResId,
            int selectedGroupResId, @Nullable OnMenuItemClickListener listener,
            boolean showShadowOnSelection, boolean updateStatusBarColor) {
        mToolbarStub.setLayoutResource(toolbarLayoutId);
        @SuppressWarnings("unchecked")
        SelectableListToolbar<E> toolbar = (SelectableListToolbar<E>) mToolbarStub.inflate();
        mToolbar = toolbar;
        mToolbar.initialize(
                delegate, titleResId, normalGroupResId, selectedGroupResId, updateStatusBarColor);

        if (listener != null) {
            mToolbar.setOnMenuItemClickListener(listener);
        }

        mToolbarShadow = (FadingShadowView) findViewById(R.id.shadow);
        mToolbarShadow.init(
                ApiCompatibilityUtils.getColor(getResources(), R.color.toolbar_shadow_color),
                FadingShadow.POSITION_TOP);

        mShowShadowOnSelection = showShadowOnSelection;
        delegate.addObserver(this);
        setToolbarShadowVisibility();

        return mToolbar;
    }

    /**
     * Initializes the view shown when the selectable list is empty.
     *
     * @param emptyStringResId The string to show when the selectable list is empty.
     * @param searchEmptyStringResId The string to show when the selectable list is empty during
     *                               a search.
     * @return The {@link TextView} displayed when the list is empty.
     */
    public TextView initializeEmptyView(int emptyStringResId, int searchEmptyStringResId) {
        mEmptyStringResId = emptyStringResId;
        mSearchEmptyStringResId = searchEmptyStringResId;

        mEmptyView.setText(mEmptyStringResId);

        // Dummy listener to have the touch events dispatched to this view tree for navigation UI.
        mEmptyViewWrapper.setOnTouchListener((v, event) -> true);

        return mEmptyView;
    }

    /**
     * Called when the view that owns the SelectableListLayout is destroyed.
     */
    public void onDestroyed() {
        mAdapter.unregisterAdapterDataObserver(mAdapterObserver);
        mToolbar.getSelectionDelegate().removeObserver(this);
        mToolbar.destroy();
        mRecyclerView.setAdapter(null);
    }

    /**
     * When this layout has a wide display style, it will be width constrained to
     * {@link UiConfig#WIDE_DISPLAY_STYLE_MIN_WIDTH_DP}. If the current screen width is greater than
     * UiConfig#WIDE_DISPLAY_STYLE_MIN_WIDTH_DP, the SelectableListLayout will be visually centered
     * by adding padding to both sides.
     *
     * This method should be called after the toolbar and RecyclerView are initialized.
     */
    public void configureWideDisplayStyle() {
        mUiConfig = new UiConfig(this);
        mToolbar.configureWideDisplayStyle(mUiConfig);
        mUiConfig.addObserver(this);
    }

    /**
     * @return The {@link UiConfig} associated with this View if one has been created, or null.
     */
    @Nullable
    public UiConfig getUiConfig() {
        return mUiConfig;
    }

    @Override
    public void onDisplayStyleChanged(DisplayStyle newDisplayStyle) {
        int padding = getPaddingForDisplayStyle(newDisplayStyle, getResources());

        ViewCompat.setPaddingRelative(mRecyclerView, padding, mRecyclerView.getPaddingTop(),
                padding, mRecyclerView.getPaddingBottom());
    }

    @Override
    public void onSelectionStateChange(List<E> selectedItems) {
        setToolbarShadowVisibility();
        if (!selectedItems.isEmpty()) {
            ((HistoryNavigationLayout) findViewById(R.id.list_content)).release();
        }
    }

    /**
     * Sets the delegate object needed for history navigation logic.
     * @param delegate {@link HistoryNavigationDelegate} object.
     */
    public void setHistoryNavigationDelegate(HistoryNavigationDelegate delegate) {
        HistoryNavigationLayout layout = (HistoryNavigationLayout) findViewById(R.id.list_content);
        layout.setNavigationDelegate(delegate);
    }

    /**
     * Called when a search is starting.
     */
    public void onStartSearch() {
        mRecyclerView.setItemAnimator(null);
        mToolbarShadow.setVisibility(View.VISIBLE);
        mEmptyView.setText(mSearchEmptyStringResId);
    }

    /**
     * Called when a search has ended.
     */
    public void onEndSearch() {
        mRecyclerView.setItemAnimator(mItemAnimator);
        setToolbarShadowVisibility();
        mEmptyView.setText(mEmptyStringResId);
    }

    /**
     * @param displayStyle The current display style..
     * @param resources The {@link Resources} used to retrieve configuration and display metrics.
     * @return The lateral padding to use for the current display style.
     */
    public static int getPaddingForDisplayStyle(DisplayStyle displayStyle, Resources resources) {
        int padding = 0;
        if (displayStyle.horizontal == HorizontalDisplayStyle.WIDE) {
            int screenWidthDp = resources.getConfiguration().screenWidthDp;
            float dpToPx = resources.getDisplayMetrics().density;
            padding = (int) (((screenWidthDp - UiConfig.WIDE_DISPLAY_STYLE_MIN_WIDTH_DP) / 2.f)
                    * dpToPx);
            padding = (int) Math.max(WIDE_DISPLAY_MIN_PADDING_DP * dpToPx, padding);
        }
        return padding;
    }

    private void setToolbarShadowVisibility() {
        if (mToolbar == null || mRecyclerView == null) return;

        boolean showShadow = mRecyclerView.canScrollVertically(-1)
                || (mToolbar.getSelectionDelegate().isSelectionEnabled() && mShowShadowOnSelection);
        mToolbarShadow.setVisibility(showShadow ? View.VISIBLE : View.GONE);
    }

    /**
     * Unlike ListView or GridView, RecyclerView does not provide default empty
     * view implementation. We need to check it ourselves.
     */
    private void updateEmptyViewVisibility() {
        int visible = mAdapter.getItemCount() == 0 ? View.VISIBLE : View.GONE;
        mEmptyView.setVisibility(visible);
        mEmptyViewWrapper.setVisibility(visible);
    }

    private void updateLayout() {
        updateEmptyViewVisibility();
        if (mAdapter.getItemCount() == 0) {
            mRecyclerView.setVisibility(View.GONE);
        } else {
            mRecyclerView.setVisibility(View.VISIBLE);
        }

        mToolbar.setSearchEnabled(mAdapter.getItemCount() != 0);
    }

    @VisibleForTesting
    public View getToolbarShadowForTests() {
        return mToolbarShadow;
    }

    /**
     * Called when the user presses the back key. Note that this method is not called automatically.
     * The embedding UI must call this method
     * when a backpress is detected for the event to be handled.
     * @return Whether this event is handled.
     */
    public boolean onBackPressed() {
        SelectionDelegate selectionDelegate = mToolbar.getSelectionDelegate();
        if (selectionDelegate.isSelectionEnabled()) {
            selectionDelegate.clearSelection();
            return true;
        }

        if (mToolbar.isSearching()) {
            mToolbar.hideSearchView();
            return true;
        }

        return false;
    }
}
