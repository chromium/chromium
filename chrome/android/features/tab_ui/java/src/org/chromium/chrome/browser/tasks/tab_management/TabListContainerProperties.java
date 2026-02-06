// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.view.View;

import androidx.core.util.Function;
import androidx.core.util.Pair;

import org.chromium.base.Callback;
import org.chromium.base.supplier.MonotonicObservableSupplier;
import org.chromium.base.supplier.NonNullObservableSupplier;
import org.chromium.base.supplier.SettableNonNullObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModel.ReadableObjectPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;

import java.util.Objects;
import java.util.function.Supplier;

@NullMarked
class TabListContainerProperties {
    public static final PropertyModel.WritableBooleanPropertyKey BLOCK_TOUCH_INPUT =
            new PropertyModel.WritableBooleanPropertyKey();

    public static final PropertyModel.ReadableObjectPropertyKey<BrowserControlsStateProvider>
            BROWSER_CONTROLS_STATE_PROVIDER = new PropertyModel.WritableObjectPropertyKey<>();

    /**
     * Integer, but not {@link PropertyModel.WritableIntPropertyKey} so that we can force update on
     * the same value.
     */
    public static final PropertyModel.WritableObjectPropertyKey<Integer> INITIAL_SCROLL_INDEX =
            new PropertyModel.WritableObjectPropertyKey<>(true);

    /** Same as {@link TabListCoordinator.TabListMode}. */
    public static final PropertyModel.WritableIntPropertyKey MODE =
            new PropertyModel.WritableIntPropertyKey();

    /**
     * A property which is set to focus on the passed tab index for accessibility. Integer, but not
     * {@link PropertyModel.WritableIntPropertyKey} so that we can focus on the same tab index which
     * may have lost focus in between.
     */
    public static final PropertyModel.WritableObjectPropertyKey<Integer>
            FOCUS_TAB_INDEX_FOR_ACCESSIBILITY =
                    new PropertyModel.WritableObjectPropertyKey<>(/* skipEquality= */ true);

    /** Sets the bottom padding for the recycler view. */
    public static final PropertyModel.WritableIntPropertyKey BOTTOM_PADDING =
            new PropertyModel.WritableIntPropertyKey();

    /** Call {@link android.view.ViewGroup#setClipToPadding(boolean)} for the view. */
    public static final PropertyModel.WritableBooleanPropertyKey IS_CLIP_TO_PADDING =
            new PropertyModel.WritableBooleanPropertyKey();

    /** Get root view for a given recycler view index. Can be null. */
    public static final ReadableObjectPropertyKey<Callback<Function<Integer, View>>>
            FETCH_VIEW_BY_INDEX_CALLBACK = new ReadableObjectPropertyKey<>();

    /** Inclusive start and stop indexes for fully visible items. */
    public static final ReadableObjectPropertyKey<Callback<Supplier<Pair<Integer, Integer>>>>
            GET_VISIBLE_RANGE_CALLBACK = new ReadableObjectPropertyKey<>();

    /** Whether the recycler view is currently being scrolled. */
    public static final ReadableObjectPropertyKey<Callback<MonotonicObservableSupplier<Boolean>>>
            IS_SCROLLING_SUPPLIER_CALLBACK = new WritableObjectPropertyKey<>();

    /** Whether the tab switcher pane has sensitive content. */
    public static final PropertyModel.WritableBooleanPropertyKey IS_CONTENT_SENSITIVE =
            new PropertyModel.WritableBooleanPropertyKey();

    public static final ReadableObjectPropertyKey<Callback<TabKeyEventData>> PAGE_KEY_LISTENER =
            new ReadableObjectPropertyKey<>();

    public static final PropertyModel.WritableBooleanPropertyKey SUPPRESS_ACCESSIBILITY =
            new PropertyModel.WritableBooleanPropertyKey();

    public static final PropertyModel.WritableBooleanPropertyKey IS_TABLET_OR_LANDSCAPE =
            new PropertyModel.WritableBooleanPropertyKey();

    public static final PropertyModel.WritableBooleanPropertyKey IS_NON_ZERO_Y_OFFSET =
            new PropertyModel.WritableBooleanPropertyKey();

    public static final WritableObjectPropertyKey<NonNullObservableSupplier<Boolean>>
            IS_PINNED_TAB_STRIP_ANIMATING_SUPPLIER = new WritableObjectPropertyKey<>();

    public static final WritableObjectPropertyKey<SettableNonNullObservableSupplier<Boolean>>
            MANUAL_SEARCH_BOX_ANIMATION_SUPPLIER = new WritableObjectPropertyKey<>();

    public static final WritableObjectPropertyKey<SettableNonNullObservableSupplier<Boolean>>
            HUB_SEARCH_BOX_VISIBILITY_SUPPLIER = new WritableObjectPropertyKey<>();

    public static final WritableObjectPropertyKey<SettableNonNullObservableSupplier<Float>>
            SEARCH_BOX_VISIBILITY_FRACTION_SUPPLIER = new WritableObjectPropertyKey<>();

    /**
     * A holder for supplementary container animation metadata, used with {@link
     * #ANIMATE_SUPPLEMENTARY_CONTAINER}.
     */
    public static final class SupplementaryContainerAnimationMetadata {
        /** Whether the search box should be shown. */
        public final boolean shouldShowSearchBox;

        /** Whether to force the animation even if the view is already in the target state. */
        public final boolean forced;

        public SupplementaryContainerAnimationMetadata(
                boolean shouldShowSearchBox, boolean forced) {
            this.shouldShowSearchBox = shouldShowSearchBox;
            this.forced = forced;
        }

        @Override
        public boolean equals(Object o) {
            // 1. Reference check.
            if (this == o) return true;

            // 2. Type and field comparison.
            if (o instanceof SupplementaryContainerAnimationMetadata that) {
                return this.shouldShowSearchBox == that.shouldShowSearchBox
                        && this.forced == that.forced;
            }

            return false;
        }

        @Override
        public int hashCode() {
            return Objects.hash(shouldShowSearchBox, forced);
        }
    }

    public static final WritableObjectPropertyKey<SupplementaryContainerAnimationMetadata>
            ANIMATE_SUPPLEMENTARY_CONTAINER = new WritableObjectPropertyKey<>();

    /** Keys for {@link TabSwitcherPaneCoordinator}. */
    public static final PropertyKey[] ALL_KEYS =
            new PropertyKey[] {
                BLOCK_TOUCH_INPUT,
                BROWSER_CONTROLS_STATE_PROVIDER,
                INITIAL_SCROLL_INDEX,
                MODE,
                FOCUS_TAB_INDEX_FOR_ACCESSIBILITY,
                BOTTOM_PADDING,
                IS_CLIP_TO_PADDING,
                FETCH_VIEW_BY_INDEX_CALLBACK,
                GET_VISIBLE_RANGE_CALLBACK,
                IS_SCROLLING_SUPPLIER_CALLBACK,
                IS_CONTENT_SENSITIVE,
                PAGE_KEY_LISTENER,
                SUPPRESS_ACCESSIBILITY,
                IS_TABLET_OR_LANDSCAPE,
                IS_NON_ZERO_Y_OFFSET,
                IS_PINNED_TAB_STRIP_ANIMATING_SUPPLIER,
                ANIMATE_SUPPLEMENTARY_CONTAINER,
                MANUAL_SEARCH_BOX_ANIMATION_SUPPLIER,
                HUB_SEARCH_BOX_VISIBILITY_SUPPLIER,
                SEARCH_BOX_VISIBILITY_FRACTION_SUPPLIER
            };
}
