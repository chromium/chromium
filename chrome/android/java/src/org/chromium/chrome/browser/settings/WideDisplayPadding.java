// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.settings;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.app.Activity;
import android.content.Context;
import android.content.res.TypedArray;
import android.graphics.drawable.Drawable;
import android.view.View;
import android.view.View.MeasureSpec;

import androidx.fragment.app.Fragment;
import androidx.preference.PreferenceFragmentCompat;
import androidx.recyclerview.widget.RecyclerView;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.components.browser_ui.settings.CustomDividerFragment;
import org.chromium.components.browser_ui.settings.PaddedItemDecorationWithDivider;
import org.chromium.components.browser_ui.widget.displaystyle.UiConfig;
import org.chromium.components.browser_ui.widget.displaystyle.ViewResizer;
import org.chromium.components.browser_ui.widget.displaystyle.ViewResizerUtil;

import java.util.function.BooleanSupplier;
import java.util.function.Supplier;

/** Applies the padding to the fragment for wide displays. */
@NullMarked
public class WideDisplayPadding {
    private static final int INVALID_ITEM_OFFSET_PX = -1;

    private final Context mContext;
    private final Fragment mFragment;
    private final @Nullable View mContent;
    private final int mMinWidePaddingPixels;
    private final UiConfig mUiConfig;
    private final BooleanSupplier mIsTwoColumnSettingsVisibleSupplier;
    private final int mMinPaddingPx;

    private int mLastAppliedItemOffsetPx = INVALID_ITEM_OFFSET_PX;

    private WideDisplayPadding(
            Fragment fragment,
            BooleanSupplier isTwoColumnSettingsVisibleSupplier,
            int minPaddingPx) {
        mContext = fragment.requireContext();
        mFragment = fragment;
        mContent = fragment.getView();
        mIsTwoColumnSettingsVisibleSupplier = isTwoColumnSettingsVisibleSupplier;
        mMinPaddingPx = minPaddingPx;

        mMinWidePaddingPixels =
                mContext.getResources()
                        .getDimensionPixelSize(R.dimen.settings_wide_display_min_padding);

        // For settings with a recycler view, add paddings to the side so the content is
        // scrollable; otherwise, add the padding to the content.
        assumeNonNull(mContent);
        RecyclerView recyclerView = mContent.findViewById(R.id.recycler_view);
        View paddedView = recyclerView == null ? mContent : recyclerView;
        mUiConfig = new UiConfig(paddedView);

        boolean hasPreferenceRecyclerView =
                fragment instanceof PreferenceFragmentCompat && recyclerView != null;
        if (hasPreferenceRecyclerView) {
            // Invalidate decorations to reset.
            mUiConfig.addObserver(
                    (newDisplayStyle) -> {
                        mLastAppliedItemOffsetPx = INVALID_ITEM_OFFSET_PX;
                        recyclerView.invalidateItemDecorations();
                    });
            recyclerView.addOnLayoutChangeListener(
                    (v, l, t, r, b, oldL, oldT, oldR, oldB) -> {
                        int newOffset = computeItemOffsetPx(recyclerView);
                        if (mLastAppliedItemOffsetPx == INVALID_ITEM_OFFSET_PX
                                || newOffset == mLastAppliedItemOffsetPx) {
                            mLastAppliedItemOffsetPx = newOffset;
                            return;
                        }
                        mLastAppliedItemOffsetPx = newOffset;
                        RecyclerView.ItemAnimator itemAnimator = recyclerView.getItemAnimator();
                        if (itemAnimator != null && recyclerView.isAnimating()) {
                            itemAnimator.endAnimations();
                        }
                        recyclerView.invalidateItemDecorations();
                        relayout(recyclerView, l, t, r, b);
                    });
        }

        // Update padding on configuration changes.
        fragment.requireActivity()
                .addOnConfigurationChangedListener(
                        (newConfig) -> {
                            mUiConfig.updateDisplayStyle();
                        });

        if (!hasPreferenceRecyclerView) {
            // Fragments without a preference RecyclerView (e.g. ListFragment or plain Fragment
            // based settings pages) have the padding applied directly to their view.
            // TODO(crbug.com/454247949): Short term workaround until margin for views are
            // updated.
            int defaultPadding =
                    mContext.getResources()
                            .getDimensionPixelSize(R.dimen.settings_single_column_layout_margin);
            new SettingsViewResizer(paddedView, mUiConfig, defaultPadding, mMinWidePaddingPixels)
                    .attach();
            return;
        }

        // Configure divider style if the fragment has a recycler view.
        // Remove the default divider that PreferenceFragmentCompat initialized. This is a
        // workaround as outer class has no access to the private DividerDecoration in
        // PreferenceFragmentCompat. See https://crbug.com/40819977.
        ((PreferenceFragmentCompat) fragment).setDivider(null);

        CustomDividerFragment customDividerFragment =
                (fragment instanceof CustomDividerFragment dividerFragment)
                        ? dividerFragment
                        : null;

        PaddedItemDecorationWithDivider itemDecoration =
                getPaddedItemDecorationWithDivider(recyclerView);
        Drawable dividerDrawable = getDividerDrawable();

        // Early return if (a)Fragment implements CustomDividerFragment and explicitly don't
        // want a divider OR (b) dividerDrawable not defined.
        if ((customDividerFragment != null && !customDividerFragment.hasDivider())
                || dividerDrawable == null) {
            recyclerView.addItemDecoration(itemDecoration);
            return;
        }

        // Configure the customized divider for the rest of the Fragments.
        Supplier<Integer> dividerStartPaddingSupplier =
                () ->
                        customDividerFragment != null
                                ? customDividerFragment.getDividerStartPadding()
                                : 0;
        Supplier<Integer> dividerEndPaddingSupplier =
                () ->
                        customDividerFragment != null
                                ? customDividerFragment.getDividerEndPadding()
                                : 0;
        itemDecoration.setDividerWithPadding(
                dividerDrawable, dividerStartPaddingSupplier, dividerEndPaddingSupplier);
        recyclerView.addItemDecoration(itemDecoration);
    }

    private PaddedItemDecorationWithDivider getPaddedItemDecorationWithDivider(
            RecyclerView recyclerView) {
        Supplier<Integer> itemOffsetSupplier =
                () -> {
                    int offset = computeItemOffsetPx(recyclerView);
                    if (mLastAppliedItemOffsetPx == INVALID_ITEM_OFFSET_PX) {
                        mLastAppliedItemOffsetPx = offset;
                    }
                    return offset;
                };
        PaddedItemDecorationWithDivider itemDecoration =
                new PaddedItemDecorationWithDivider(itemOffsetSupplier);
        return itemDecoration;
    }

    private int computeItemOffsetPx(RecyclerView recyclerView) {
        if (mIsTwoColumnSettingsVisibleSupplier.getAsBoolean()) {
            return computeMultiColumnSearchPadding(recyclerView);
        }
        return ViewResizerUtil.computePadding(
                recyclerView, mUiConfig, /* defaultPaddingPixels= */ 0, mMinWidePaddingPixels);
    }

    /**
     * Computes the horizontal padding that centers the content within the two-column detail pane.
     *
     * @param view The view whose width defines the available space.
     */
    private int computeMultiColumnSearchPadding(View view) {
        int widthPx = getAvailableWidthPx(view);
        if (widthPx == 0) return mMinPaddingPx;

        int maxDetailWidthPx =
                mContext.getResources()
                        .getDimensionPixelSize(R.dimen.settings_min_multi_column_screen_width);
        int excessPx = widthPx - maxDetailWidthPx - mMinPaddingPx * 2;
        return mMinPaddingPx + (excessPx > 0 ? excessPx / 2 : 0);
    }

    /**
     * Returns the available width for the view in pixels.
     *
     * <p>When called before the fragment's view has been laid out, falls back to the parent view,
     * the detail pane container, or the window width minus the narrow header width so that the
     * initial frame can render with the correct padding.
     */
    private int getAvailableWidthPx(View view) {
        int widthPx = view.getWidth();
        if (widthPx > 0) return widthPx;

        // Fallback 1: View has not been laid out yet, but its parent has (e.g. navigating to
        // a new fragment while the container is already laid out).
        if (view.getParent() instanceof View parent && parent.getWidth() > 0) {
            return parent.getWidth();
        }

        if (!mFragment.isAdded()) return 0;

        Activity activity = mFragment.getActivity();
        if (activity == null) return 0;

        // Fallback 2: The parent is not yet attached, but the detail pane container is laid out.
        View detail = activity.findViewById(R.id.preferences_detail);
        if (detail != null && detail.getWidth() > 0) {
            return detail.getWidth();
        }

        // Fallback 3: Cold start before the activity layout pass. In two-column mode the header
        // pane has a fixed width (@dimen/settings_narrow_header_width) and the detail pane takes
        // the remaining window width.
        View decorView = activity.getWindow() != null ? activity.getWindow().getDecorView() : null;
        int totalWidth =
                decorView != null && decorView.getWidth() > 0
                        ? decorView.getWidth()
                        : mContext.getResources().getDisplayMetrics().widthPixels;
        int headerWidth =
                mContext.getResources().getDimensionPixelSize(R.dimen.settings_narrow_header_width);
        return Math.max(0, totalWidth - headerWidth);
    }

    /**
     * Applies the padding to the fragment for wide displays.
     *
     * <p>Call this method exactly once with a top-level fragment on its creation.
     *
     * @param fragment The fragment to apply padding to.
     * @param isTwoColumnSettingsVisibleSupplier Supplier to check if two-column settings is
     *     visible.
     * @param minPaddingPx Minimum horizontal padding to apply to the fragment in pixels.
     */
    public static void apply(
            Fragment fragment,
            BooleanSupplier isTwoColumnSettingsVisibleSupplier,
            int minPaddingPx) {
        new WideDisplayPadding(fragment, isTwoColumnSettingsVisibleSupplier, minPaddingPx);
    }

    /**
     * Re-measures and re-lays out {@code view} within the current layout pass.
     *
     * <p>View.layout() clears PFLAG_FORCE_LAYOUT right after notifying layout change listeners, so
     * a requestLayout() made from a listener (e.g. by setPaddingRelative() or
     * invalidateItemDecorations()) is dropped and the children would stay positioned for the old
     * padding or item offset. Lay them out again right away so the stale layout is never drawn or
     * captured by transitions. See crbug.com/565658549.
     */
    private static void relayout(View view, int left, int top, int right, int bottom) {
        view.measure(
                MeasureSpec.makeMeasureSpec(right - left, MeasureSpec.EXACTLY),
                MeasureSpec.makeMeasureSpec(bottom - top, MeasureSpec.EXACTLY));
        view.layout(left, top, right, bottom);
    }

    // Get the divider drawable from AndroidX Pref attribute to keep things consistent.
    private @Nullable Drawable getDividerDrawable() {
        TypedArray ta =
                mContext.obtainStyledAttributes(
                        null,
                        R.styleable.PreferenceFragmentCompat,
                        R.attr.preferenceFragmentCompatStyle,
                        0);
        final Drawable divider =
                ta.getDrawable(R.styleable.PreferenceFragmentCompat_android_divider);
        ta.recycle();

        return divider;
    }

    /**
     * A {@link ViewResizer} that additionally centers the content when the two-column settings
     * layout is visible.
     *
     * <p>In two-column mode the fragment fills the detail pane rather than the window, so the
     * padding must be derived from the pane width. {@link ViewResizer} recomputes the padding on
     * every layout pass, which also covers the transition between one and two columns when the
     * window is resized.
     */
    private class SettingsViewResizer extends ViewResizer {
        private final View mResizedView;
        private final int mContainmentHorizontalMarginPx;

        SettingsViewResizer(
                View view, UiConfig uiConfig, int defaultPaddingPixels, int minWidePaddingPixels) {
            super(view, uiConfig, defaultPaddingPixels, minWidePaddingPixels);
            mResizedView = view;
            mContainmentHorizontalMarginPx =
                    mContext.getResources().getDimensionPixelSize(R.dimen.settings_item_margin);
            applyPadding(computePadding());
        }

        @Override
        protected int computePadding() {
            if (mIsTwoColumnSettingsVisibleSupplier.getAsBoolean()) {
                // SettingsContainmentHelper zeroes the containment horizontal margin for
                // preference pages in two-column mode, so their containment rectangles line up
                // exactly with the padding computed here. Pages padded as a whole still apply
                // that margin to their own items, so subtract it to keep the rectangles
                // aligned across settings pages.
                int padding = computeMultiColumnSearchPadding(mResizedView);
                return Math.max(0, padding - mContainmentHorizontalMarginPx);
            }
            return super.computePadding();
        }

        @Override
        public void onLayoutChange(
                View view,
                int left,
                int top,
                int right,
                int bottom,
                int oldLeft,
                int oldTop,
                int oldRight,
                int oldBottom) {
            // Padding is initialized before the first layout pass so the initial frame renders
            // with the correct padding without visual glitch. Only update when the padding has
            // changed (e.g. during a window resize).
            int padding = computePadding();
            if (padding == view.getPaddingStart()) return;
            applyPadding(padding);

            // The requestLayout() triggered by setPaddingRelative() is dropped here. This happens
            // when the initial padding was estimated from the window width because no ancestor
            // had been measured yet, for example when settings is opened in a new tab next to the
            // vertical tab strip. See crbug.com/565658549.
            relayout(view, left, top, right, bottom);
        }

        private void applyPadding(int padding) {
            mResizedView.setPaddingRelative(
                    padding,
                    mResizedView.getPaddingTop(),
                    padding,
                    mResizedView.getPaddingBottom());
        }
    }
}
