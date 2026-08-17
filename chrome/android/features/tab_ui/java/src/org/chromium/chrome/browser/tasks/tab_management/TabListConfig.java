// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import org.chromium.base.supplier.NonNullObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.tasks.tab_management.TabListMediator.TabListLayoutType;
import org.chromium.chrome.browser.tasks.tab_management.vertical_tabs.VerticalTabHoverCardController.TabHoverCardListener;
import org.chromium.chrome.browser.tasks.tab_management.vertical_tabs.VerticalTabListProperties.RailCollapseState;

/**
 * Configuration policies and visual capabilities for the TabList. The returned values are immutable
 * once constructed.
 *
 * <p>TODO(crbug.com/509226293): Migrate remaining TabListMode capabilities here to eliminate mMode
 * from TabListMediator.
 */
@NullMarked
public class TabListConfig {
    /** The layout organization type for the TabList. */
    public final @TabListLayoutType int layoutType;

    /** Whether the layout supports message card items. */
    public final boolean supportsMessageCards;

    /** Supplier for the rail collapse state in vertical tabs, or null if not supported. */
    public final @Nullable NonNullObservableSupplier<@RailCollapseState Integer>
            railCollapseStateSupplier;

    /** Listener for tab hover card events, or null if not supported. */
    public final @Nullable TabHoverCardListener tabHoverCardListener;

    private TabListConfig(Builder builder) {
        layoutType = builder.mLayoutType;
        supportsMessageCards = builder.mSupportsMessageCards;
        railCollapseStateSupplier = builder.mRailCollapseStateSupplier;
        tabHoverCardListener = builder.mTabHoverCardListener;
    }

    /** Builder to construct {@link TabListConfig}. */
    public static class Builder {
        private final @TabListLayoutType int mLayoutType;
        private boolean mSupportsMessageCards;
        private @Nullable NonNullObservableSupplier<@RailCollapseState Integer>
                mRailCollapseStateSupplier;
        private @Nullable TabHoverCardListener mTabHoverCardListener;

        /**
         * @param layoutType The {@link TabListLayoutType} for the tab list.
         */
        public Builder(@TabListLayoutType int layoutType) {
            mLayoutType = layoutType;
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
         * @param tabHoverCardListener Listener for tab hover card events, or null.
         * @return The {@link Builder} instance.
         */
        public Builder setTabHoverCardListener(
                @Nullable TabHoverCardListener tabHoverCardListener) {
            mTabHoverCardListener = tabHoverCardListener;
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
