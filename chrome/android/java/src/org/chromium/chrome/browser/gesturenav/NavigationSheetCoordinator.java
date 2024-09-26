// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.gesturenav;

import android.content.Context;
import android.os.Handler;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.ImageView;
import android.widget.ListView;
import android.widget.TextView;

import androidx.annotation.DimenRes;

import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.gesturenav.NavigationSheetMediator.ItemProperties;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.SheetState;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.StateChangeReason;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetObserver;
import org.chromium.components.browser_ui.bottomsheet.EmptyBottomSheetObserver;
import org.chromium.content_public.browser.NavigationHistory;
import org.chromium.ui.modelutil.LayoutViewBuilder;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.ModelListAdapter;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * Coordinator class for navigation sheet.
 * TODO(jinsukkim): Write tests.
 */
class NavigationSheetCoordinator implements BottomSheetContent, NavigationSheet {
    // Type of the navigation list item. We have only single type.
    static final int NAVIGATION_LIST_ITEM_TYPE_ID = 0;

    // Amount of time to hold the finger still to trigger navigation bottom sheet.
    // with a long swipe. This ensures fling gestures from edge won't invoke the  sheet.
    private static final int LONG_SWIPE_HOLD_DELAY_MS = 50;

    // Amount of time to hold the finger still to trigger navigation bottom sheet
    // with a short swipe.
    private static final int SHORT_SWIPE_HOLD_DELAY_MS = 400;

    // Amount of distance to trigger navigation sheet with a long swipe.
    // Actual amount is capped so it is at most half the screen width.
    private static final int LONG_SWIPE_PEEK_THRESHOLD_DP = 224;

    // The history item count in the navigation sheet. If the count is equal or smaller,
    // the sheet skips peek state and fully expands right away.
    private static final int SKIP_PEEK_COUNT = 3;

    // Delta for touch events that can happen even when users doesn't intend to move
    // his finger. Any delta smaller than (or equal to) than this are ignored.
    private static final float DELTA_IGNORE = 2.f;

    private final View mToolbarView;
    private final LayoutInflater mLayoutInflater;
    private final Supplier<BottomSheetController> mBottomSheetController;
    private final NavigationSheetMediator mMediator;
    private final BottomSheetObserver mSheetObserver =
            new EmptyBottomSheetObserver() {
                @Override
                public void onSheetClosed(@StateChangeReason int reason) {
                    close(false);
                }
            };

    private final Handler mHandler = new Handler();
    private final Runnable mOpenSheetRunnable;
    private final float mLongSwipePeekThreshold;

    private final ModelList mModelList = new ModelList();
    private final ModelListAdapter mModelAdapter = new ModelListAdapter(mModelList);

    private final int mItemHeight;
    private final int mContentPadding;
    private final View mParentView;

    private NavigationSheet.Delegate mDelegate;

    private static class NavigationItemViewBinder {
        public static void bind(PropertyModel model, View view, PropertyKey propertyKey) {
            if (ItemProperties.ICON == propertyKey) {
                ((ImageView) view.findViewById(R.id.favicon_img))
                        .setImageDrawable(model.get(ItemProperties.ICON));
            } else if (ItemProperties.LABEL == propertyKey) {
                ((TextView) view.findViewById(R.id.entry_title))
                        .setText(model.get(ItemProperties.LABEL));
            } else if (ItemProperties.CLICK_LISTENER == propertyKey) {
                view.setOnClickListener(model.get(ItemProperties.CLICK_LISTENER));
            }
        }
    }

    private NavigationSheetView mContentView;

    private boolean mForward;

    private boolean mShowCloseIndicator;

    // Metrics. True if sheet was opened from long-press on back button.
    private boolean mOpenedAsPopup;

    // Set to {@code true} for each trigger when the sheet should fully expand with
    // no peek/half state.
    private boolean mFullyExpand;

    private Profile mProfile;

    /** Construct a new NavigationSheet. */
    NavigationSheetCoordinator(
            View parent,
            Context context,
            Supplier<BottomSheetController> bottomSheetController,
            Profile profile) {
        mParentView = parent;
        mBottomSheetController = bottomSheetController;
        mLayoutInflater = LayoutInflater.from(context);
        mToolbarView = mLayoutInflater.inflate(R.layout.navigation_sheet_toolbar, null);
        mProfile = profile;
        mMediator =
                new NavigationSheetMediator(
                        context,
                        mModelList,
                        profile,
                        (position, index) -> {
                            mDelegate.navigateToIndex(index);
                            close(false);
                            if (mOpenedAsPopup) {
                                GestureNavMetrics.recordUserAction(
                                        (index == -1)
                                                ? "ShowFullHistory"
                                                : "HistoryClick" + (position + 1));
                            }
                        });
        mModelAdapter.registerType(
                NAVIGATION_LIST_ITEM_TYPE_ID,
                new LayoutViewBuilder(R.layout.navigation_popup_item),
                NavigationItemViewBinder::bind);
        mOpenSheetRunnable =
                () -> {
                    if (isHidden()) openSheet(true, true);
                };
        mLongSwipePeekThreshold =
                Math.min(
                        context.getResources().getDisplayMetrics().density
                                * LONG_SWIPE_PEEK_THRESHOLD_DP,
                        parent.getWidth() / 2f);
        mItemHeight = getSizePx(context, R.dimen.navigation_popup_item_height);
        mContentPadding =
                getSizePx(context, R.dimen.navigation_sheet_content_top_padding)
                        + getSizePx(context, R.dimen.navigation_sheet_content_bottom_padding);
    }

    private static int getSizePx(Context context, @DimenRes int id) {
        return context.getResources().getDimensionPixelSize(id);
    }

    // Transition to either peeked or expanded state.
    private boolean openSheet(boolean expandIfSmall, boolean animate) {
        mContentView =
                (NavigationSheetView) mLayoutInflater.inflate(R.layout.navigation_sheet, null);
        ListView listview = mContentView.findViewById(R.id.navigation_entries);
        listview.setAdapter(mModelAdapter);
        NavigationHistory history = mDelegate.getHistory(mForward, mProfile.isOffTheRecord());
        // If there is no entry, the sheet should not be opened. This is the case when in a fresh
        // Incognito NTP.
        if (history.getEntryCount() == 0) return false;
        mMediator.populateEntries(history);
        if (!mBottomSheetController.get().requestShowContent(this, true)) {
            close(false);
            mContentView = null;
            return false;
        }
        mBottomSheetController.get().addObserver(mSheetObserver);
        if (expandIfSmall && history.getEntryCount() <= SKIP_PEEK_COUNT) {
            mFullyExpand = true;
            expandSheet();
        }
        return true;
    }

    private void expandSheet() {
        mBottomSheetController.get().expandSheet();
    }

    // NavigationSheet

    @Override
    public void setDelegate(NavigationSheet.Delegate delegate) {
        mDelegate = delegate;
    }

    @Override
    public void start(boolean forward, boolean showCloseIndicator) {
        if (mBottomSheetController.get() == null) return;
        mForward = forward;
        mShowCloseIndicator = showCloseIndicator;
        mFullyExpand = false;
        mOpenedAsPopup = false;
    }

    @Override
    public boolean startAndExpand(boolean forward, boolean animate) {
        // Called from activity for navigation popup. No need to check
        // bottom sheet controller since it is guaranteed to available.
        start(forward, /* showCloseIndicator= */ false);
        mOpenedAsPopup = true;

        // Enter the expanded state by disabling peek/half state rather than
        // calling |expandSheet| explicilty. Otherwise it cause an extra
        // state transition (full -> full), which cancels the animation effect.
        boolean opened = openSheet(/* expandIfSmall= */ false, animate);
        if (opened) GestureNavMetrics.recordUserAction("Popup");
        return opened;
    }

    @Override
    public void onScroll(float delta, float overscroll, boolean willNavigate) {
        if (mBottomSheetController.get() == null) return;
        if (mShowCloseIndicator) return;
        if (overscroll > mLongSwipePeekThreshold) {
            triggerSheetWithSwipeAndHold(delta, LONG_SWIPE_HOLD_DELAY_MS);
        } else if (willNavigate) {
            triggerSheetWithSwipeAndHold(delta, SHORT_SWIPE_HOLD_DELAY_MS);
        } else if (isPeeked()) {
            close(true);
        } else {
            mHandler.removeCallbacks(mOpenSheetRunnable);
        }
    }

    private void triggerSheetWithSwipeAndHold(float delta, long delay) {
        if (isHidden() && Math.abs(delta) > DELTA_IGNORE) {
            mHandler.removeCallbacks(mOpenSheetRunnable);
            mHandler.postDelayed(mOpenSheetRunnable, delay);
        }
    }

    @Override
    public void release() {
        if (mBottomSheetController.get() == null) return;
        mHandler.removeCallbacks(mOpenSheetRunnable);

        // Show navigation sheet if released at peek state.
        if (isPeeked()) expandSheet();
    }

    @Override
    public void close(boolean animate) {
        BottomSheetController controller = mBottomSheetController.get();
        if (controller == null) return;
        controller.hideContent(this, animate);
        controller.removeObserver(mSheetObserver);
        mMediator.clear();
    }

    @Override
    public boolean isHidden() {
        if (mBottomSheetController.get() == null) return true;
        return getTargetOrCurrentState() == BottomSheetController.SheetState.HIDDEN;
    }

    /**
     * @return {@code true} if the sheet is in peeked state.
     */
    private boolean isPeeked() {
        if (mBottomSheetController.get() == null) return false;
        return getTargetOrCurrentState() == BottomSheetController.SheetState.PEEK;
    }

    private @SheetState int getTargetOrCurrentState() {
        @SheetState int state = mBottomSheetController.get().getTargetSheetState();
        return state != BottomSheetController.SheetState.NONE
                ? state
                : mBottomSheetController.get().getSheetState();
    }

    @Override
    public boolean isExpanded() {
        if (mBottomSheetController.get() == null) return false;
        int state = getTargetOrCurrentState();
        return state == BottomSheetController.SheetState.HALF
                || state == BottomSheetController.SheetState.FULL;
    }

    // BottomSheetContent

    @Override
    public View getContentView() {
        return mContentView;
    }

    @Override
    public View getToolbarView() {
        return mToolbarView;
    }

    @Override
    public int getVerticalScrollOffset() {
        return mContentView.getVerticalScrollOffset();
    }

    @Override
    public void destroy() {}

    @Override
    public @ContentPriority int getPriority() {
        return ContentPriority.LOW;
    }

    @Override
    public boolean swipeToDismissEnabled() {
        return true;
    }

    @Override
    public int getPeekHeight() {
        if (mBottomSheetController.get() == null || mOpenedAsPopup) {
            return BottomSheetContent.HeightMode.DISABLED;
        }
        // Makes peek state as 'not present' when bottom sheet is in expanded state (i.e. animating
        // from expanded to close state). It avoids the sheet animating in two distinct steps, which
        // looks awkward.
        return !mBottomSheetController.get().isSheetOpen()
                ? getSizePx(mParentView.getContext(), R.dimen.navigation_sheet_peek_height)
                : BottomSheetContent.HeightMode.DISABLED;
    }

    @Override
    public float getHalfHeightRatio() {
        if (mOpenedAsPopup) return BottomSheetContent.HeightMode.DISABLED;
        return getCappedHeightRatio(mParentView.getHeight() / 2 + mItemHeight / 2);
    }

    @Override
    public float getFullHeightRatio() {
        return getCappedHeightRatio(mParentView.getHeight());
    }

    private float getCappedHeightRatio(float maxHeight) {
        int entryCount = mModelAdapter.getCount();
        return Math.min(maxHeight, entryCount * mItemHeight + mContentPadding)
                / mParentView.getHeight();
    }

    @Override
    public int getSheetContentDescriptionStringId() {
        return R.string.overscroll_navigation_sheet_description;
    }

    @Override
    public int getSheetHalfHeightAccessibilityStringId() {
        return R.string.overscroll_navigation_sheet_opened_half;
    }

    @Override
    public int getSheetFullHeightAccessibilityStringId() {
        return R.string.overscroll_navigation_sheet_opened_full;
    }

    @Override
    public int getSheetClosedAccessibilityStringId() {
        return R.string.overscroll_navigation_sheet_closed;
    }
}
