// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import org.chromium.base.supplier.NonNullObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.compositor.overlays.strip.TabUnderlineManager;
import org.chromium.chrome.browser.tabmodel.TabClosingSource;
import org.chromium.chrome.browser.tasks.tab_management.TabListMediator.TabListLayoutType;
import org.chromium.chrome.browser.tasks.tab_management.TabProperties.UiType;
import org.chromium.chrome.browser.tasks.tab_management.vertical_tabs.VerticalTabHoverCardController.TabHoverCardListener;
import org.chromium.chrome.browser.tasks.tab_management.vertical_tabs.VerticalTabListProperties.RailCollapseState;

/**
 * Configuration policies and visual capabilities for the TabList. The returned values are immutable
 * once constructed.
 */
@NullMarked
public class TabListConfig {
    /** The layout organization type for the TabList. */
    public final @TabListLayoutType int layoutType;

    /**
     * The base card UI type for tabs in the list (e.g. {@link UiType#TAB} or {@link UiType#STRIP}).
     */
    public final @UiType int tabUiType;

    /**
     * Whether the layout supports message card items (e.g. IPH, promo cards, price welcome cards).
     * Currently only the Tab Switcher Grid ({@link TabListLayoutType#GROUPED}) uses message cards
     * and price tracking. If a non-grid layout needs price tracking independently in the future,
     * add a dedicated capability flag.
     */
    public final boolean supportsMessageCards;

    /** Whether the tab list supports modifier-based multi-selection (e.g. Ctrl/Shift+Click). */
    public final boolean supportsModifierMultiSelect;

    /** Whether the tab list items support displaying a loading state / spinner. */
    public final boolean supportsTabLoadingState;

    /** Whether the tab list supports shrink-to-close animations on tab removal. */
    public final boolean supportsShrinkCloseAnimation;

    /**
     * Whether the component delays adding tabs to the model when created from switcher/group UI
     * until after the switcher or dialog finishes hiding.
     */
    public final boolean supportsDelayedTabAddition;

    /** The {@link TabClosingSource} to attribute when tabs or tab groups are closed. */
    public final @TabClosingSource int tabClosingSource;

    // TODO(crbug.com/509226293): Revisit if vertical tabs specific fields like
    // railCollapseStateSupplier should be decoupled or moved to a dedicated mediator.
    /** Supplier for the rail collapse state in vertical tabs, or null if not supported. */
    public final @Nullable NonNullObservableSupplier<@RailCollapseState Integer>
            railCollapseStateSupplier;

    /** Listener for tab and tab group hover card events, or null if not supported. */
    public final @Nullable TabHoverCardListener tabHoverCardListener;

    /** Manager for active tab underline indicators (e.g. for Glic), or null if not supported. */
    public final @Nullable TabUnderlineManager tabUnderlineManager;

    private TabListConfig(Builder builder) {
        layoutType = builder.mLayoutType;
        tabUiType = builder.mTabUiType;
        supportsMessageCards = builder.mSupportsMessageCards;
        supportsModifierMultiSelect = builder.mSupportsModifierMultiSelect;
        supportsTabLoadingState = builder.mSupportsTabLoadingState;
        supportsShrinkCloseAnimation = builder.mSupportsShrinkCloseAnimation;
        supportsDelayedTabAddition = builder.mSupportsDelayedTabAddition;
        tabClosingSource = builder.mTabClosingSource;
        railCollapseStateSupplier = builder.mRailCollapseStateSupplier;
        tabHoverCardListener = builder.mTabHoverCardListener;
        tabUnderlineManager = builder.mTabUnderlineManager;
    }

    /** Builder to construct {@link TabListConfig}. */
    public static class Builder {
        private final @TabListLayoutType int mLayoutType;
        private @UiType int mTabUiType;
        private boolean mSupportsMessageCards;
        private boolean mSupportsModifierMultiSelect;
        private boolean mSupportsTabLoadingState;
        private boolean mSupportsShrinkCloseAnimation;
        private boolean mSupportsDelayedTabAddition;
        private @TabClosingSource int mTabClosingSource;
        private @Nullable NonNullObservableSupplier<@RailCollapseState Integer>
                mRailCollapseStateSupplier;
        private @Nullable TabHoverCardListener mTabHoverCardListener;
        private @Nullable TabUnderlineManager mTabUnderlineManager;

        /**
         * @param layoutType The {@link TabListLayoutType} for the tab list.
         */
        public Builder(@TabListLayoutType int layoutType) {
            mLayoutType = layoutType;
        }

        /**
         * Sets the base {@link UiType} for tab items in the list.
         *
         * @param tabUiType The base {@link UiType} for tab items in the list.
         * @return The {@link Builder} instance.
         */
        public Builder setTabUiType(@UiType int tabUiType) {
            mTabUiType = tabUiType;
            return this;
        }

        /**
         * @param supportsMessageCards Whether the tab list supports message cards.
         * @return The {@link Builder} instance.
         */
        public Builder setSupportsMessageCards(boolean supportsMessageCards) {
            mSupportsMessageCards = supportsMessageCards;
            return this;
        }

        /**
         * @param supportsModifierMultiSelect Whether the tab list supports modifier multi-select.
         * @return The {@link Builder} instance.
         */
        public Builder setSupportsModifierMultiSelect(boolean supportsModifierMultiSelect) {
            mSupportsModifierMultiSelect = supportsModifierMultiSelect;
            return this;
        }

        /**
         * @param supportsTabLoadingState Whether tab list items support displaying a loading state.
         * @return The {@link Builder} instance.
         */
        public Builder setSupportsTabLoadingState(boolean supportsTabLoadingState) {
            mSupportsTabLoadingState = supportsTabLoadingState;
            return this;
        }

        /**
         * Sets whether the tab list supports shrink-to-close animations on tab removal.
         *
         * @param supportsShrinkCloseAnimation Whether the tab list supports shrink-to-close
         *     animations.
         * @return The {@link Builder} instance.
         */
        public Builder setSupportsShrinkCloseAnimation(boolean supportsShrinkCloseAnimation) {
            mSupportsShrinkCloseAnimation = supportsShrinkCloseAnimation;
            return this;
        }

        /**
         * Sets whether the component delays adding tabs to the model when created from
         * switcher/group UI.
         *
         * @param supportsDelayedTabAddition Whether to delay tab addition until post-hiding.
         * @return The {@link Builder} instance.
         */
        public Builder setSupportsDelayedTabAddition(boolean supportsDelayedTabAddition) {
            mSupportsDelayedTabAddition = supportsDelayedTabAddition;
            return this;
        }

        /**
         * Sets the {@link TabClosingSource} for tab closures initiated from this tab list.
         *
         * @param tabClosingSource The {@link TabClosingSource}.
         * @return The {@link Builder} instance.
         */
        public Builder setTabClosingSource(@TabClosingSource int tabClosingSource) {
            mTabClosingSource = tabClosingSource;
            return this;
        }

        /**
         * @param railCollapseStateSupplier Supplier for rail collapse state, or null.
         * @return The {@link Builder} instance.
         */
        public Builder setRailCollapseStateSupplier(
                @Nullable NonNullObservableSupplier<@RailCollapseState Integer>
                        railCollapseStateSupplier) {
            mRailCollapseStateSupplier = railCollapseStateSupplier;
            return this;
        }

        /**
         * @param tabHoverCardListener Listener for tab and tab group hover card events, or null.
         * @return The {@link Builder} instance.
         */
        public Builder setTabHoverCardListener(
                @Nullable TabHoverCardListener tabHoverCardListener) {
            mTabHoverCardListener = tabHoverCardListener;
            return this;
        }

        /**
         * @param tabUnderlineManager Manager for active tab underline indicators, or null.
         * @return The {@link Builder} instance.
         */
        public Builder setTabUnderlineManager(@Nullable TabUnderlineManager tabUnderlineManager) {
            mTabUnderlineManager = tabUnderlineManager;
            return this;
        }

        /**
         * Builds the {@link TabListConfig} instance.
         *
         * @return The configured {@link TabListConfig}.
         */
        public TabListConfig build() {
            return new TabListConfig(this);
        }
    }
}
