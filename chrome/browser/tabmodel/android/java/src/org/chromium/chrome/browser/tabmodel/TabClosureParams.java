// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import com.google.errorprone.annotations.DoNotMock;

import org.chromium.base.Token;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.tab.Tab;

import java.util.Collections;
import java.util.List;
import java.util.Objects;

/** Parameters to control closing tabs from the {@link TabModel}. */
// TODO(crbug.com/376710475): Consider prefixing the static methods with for.
@DoNotMock("Create a real instance instead.")
@NullMarked
public class TabClosureParams {
    /** Returns a new {@link Builder} for closing a single tab. */
    public static Builder closeTab(Tab tab) {
        return new Builder(
                TabCloseType.SINGLE, /* isAllTabs= */ false, Collections.singletonList(tab));
    }

    /** Returns a new {@link Builder} for closing a list of tabs. */
    public static Builder closeTabs(List<Tab> tabs) {
        return new Builder(TabCloseType.MULTIPLE, /* isAllTabs= */ false, tabs);
    }

    /**
     * Returns a new {@link Builder} for closing an entire tab group.
     *
     * @param tabModel The tab model containing the tab group.
     * @param tabGroupId The ID of the tab group.
     * @return A builder for the tab group or null if the group is not found.
     */
    public static @Nullable Builder forCloseTabGroup(
            TabModel tabModel, @Nullable Token tabGroupId) {
        List<Tab> relatedTabs = tabModel.getTabsInGroup(tabGroupId);
        if (relatedTabs.isEmpty()) return null;

        return closeTabs(relatedTabs).isTabGroup(true);
    }

    /** Returns a new {@link Builder} for closing all tabs. */
    public static Builder closeAllTabs() {
        return new Builder(TabCloseType.ALL, /* isAllTabs= */ true, /* tabs= */ null);
    }

    /**
     * Builder for {@link TabClosureParams}.
     *
     * <p>Instances are obtained from the static factory methods on {@link TabClosureParams}. The
     * factory fixes the tabs, the {@link TabCloseType}, and whether this is an all-tabs closure;
     * none of the three can be changed afterwards. Fields that are not meaningful for the seeded
     * close type must be left at their defaults; writing any other value asserts rather than being
     * silently ignored.
     */
    public static class Builder {
        private final @TabCloseType int mTabCloseType;
        private final boolean mIsAllTabs;
        private final @Nullable List<Tab> mTabs;

        private @Nullable Tab mRecommendedNextTab;
        private boolean mUponExit;
        private boolean mAllowUndo = true;
        private boolean mHideTabGroups;
        private boolean mSaveToTabRestoreService = true;
        private @TabClosingSource int mTabClosingSource = TabClosingSource.UNKNOWN;
        private @Nullable Runnable mUndoRunnable;
        private boolean mIsTabGroup;

        private Builder(
                @TabCloseType int tabCloseType, boolean isAllTabs, @Nullable List<Tab> tabs) {
            mTabCloseType = tabCloseType;
            mIsAllTabs = isAllTabs;
            mTabs = tabs;
        }

        // Availability matrix. Each predicate answers "may this setter write this value?" for the
        // close type the builder was seeded with. A write that leaves the field unchanged is
        // always permitted, so a caller may pass through a field the close type does not support
        // as long as the value is the one already there. On an unsupported close type nothing can
        // have legally written the field, so "unchanged" and "still the default" coincide; the
        // predicates therefore never restate the defaults, which live only on the declarations
        // above.

        private boolean canSetRecommendedNextTab(@Nullable Tab recommendedNextTab) {
            return mTabCloseType == TabCloseType.SINGLE
                    || recommendedNextTab == mRecommendedNextTab;
        }

        private boolean canSetUponExit(boolean uponExit) {
            return mTabCloseType != TabCloseType.MULTIPLE || uponExit == mUponExit;
        }

        private boolean canSetHideTabGroups(boolean hideTabGroups) {
            return mTabCloseType != TabCloseType.SINGLE || hideTabGroups == mHideTabGroups;
        }

        private boolean canSetSaveToTabRestoreService(boolean saveToTabRestoreService) {
            return mTabCloseType != TabCloseType.SINGLE
                    || saveToTabRestoreService == mSaveToTabRestoreService;
        }

        private boolean canSetIsTabGroup(boolean isTabGroup) {
            return mTabCloseType == TabCloseType.MULTIPLE || isTabGroup == mIsTabGroup;
        }

        /**
         * Sets the recommended next tab to select. Default is null. Only meaningful when closing a
         * single tab; other close types must leave it at its default.
         */
        public Builder recommendedNextTab(@Nullable Tab recommendedNextTab) {
            assert canSetRecommendedNextTab(recommendedNextTab)
                    : "recommendedNextTab must be left at its default unless closing a single tab.";
            mRecommendedNextTab = recommendedNextTab;
            return this;
        }

        /**
         * Sets whether the tab closure completing would exit the app. Default is false. Closing a
         * list of tabs must leave it at its default.
         */
        public Builder uponExit(boolean uponExit) {
            assert canSetUponExit(uponExit)
                    : "uponExit must be left at its default when closing a list of tabs.";
            mUponExit = uponExit;
            return this;
        }

        /** Set whether to allow undo. Default is true. */
        public Builder allowUndo(boolean allowUndo) {
            mAllowUndo = allowUndo;
            return this;
        }

        /**
         * Set whether to hide or delete tab groups. Default is delete. Closing a single tab must
         * leave it at its default.
         */
        public Builder hideTabGroups(boolean hideTabGroups) {
            assert canSetHideTabGroups(hideTabGroups)
                    : "hideTabGroups must be left at its default when closing a single tab.";
            mHideTabGroups = hideTabGroups;
            return this;
        }

        /**
         * Set whether to allow saving to the Tab Restore Service. Default is true. Closing a
         * single tab must leave it at its default.
         */
        public Builder saveToTabRestoreService(boolean saveToTabRestoreService) {
            assert canSetSaveToTabRestoreService(saveToTabRestoreService)
                    : "saveToTabRestoreService must be left at its default when closing a single"
                            + " tab.";
            mSaveToTabRestoreService = saveToTabRestoreService;
            return this;
        }

        /** Set the tab closing source. Default is unknown. */
        public Builder tabClosingSource(@TabClosingSource int tabClosingSource) {
            mTabClosingSource = tabClosingSource;
            return this;
        }

        /** Sets the undo runnable. */
        public Builder withUndoRunnable(@Nullable Runnable undoRunnable) {
            mUndoRunnable = undoRunnable;
            return this;
        }

        /**
         * Sets whether the closure is for a tab group and came from {@link forCloseTabGroup}. This
         * is used to identify if the tab closure is for an entire tab group. It is currently used
         * by {@link TabRemover} to decide which type of dialog to show. It may have other uses in
         * the future such as ensuring all tabs in a group are closed even if the close operation is
         * deferred.
         *
         * <p>Only meaningful when closing a list of tabs; other close types must leave it at its
         * default.
         */
        private Builder isTabGroup(boolean isTabGroup) {
            assert canSetIsTabGroup(isTabGroup)
                    : "isTabGroup must be left at its default unless closing a list of tabs.";
            mIsTabGroup = isTabGroup;
            return this;
        }

        /** Builds the params. */
        public TabClosureParams build() {
            return new TabClosureParams(
                    mTabs,
                    mIsAllTabs,
                    mRecommendedNextTab,
                    mUponExit,
                    mAllowUndo,
                    mHideTabGroups,
                    mSaveToTabRestoreService,
                    mTabClosingSource,
                    mTabCloseType,
                    mUndoRunnable,
                    mIsTabGroup);
        }
    }

    // TODO(crbug.com/356445932): Consider package protecting these fields.
    public final @Nullable List<Tab> tabs;
    public final boolean isAllTabs;
    public final @Nullable Tab recommendedNextTab;
    public final boolean uponExit;
    public final boolean allowUndo;
    public final boolean hideTabGroups;
    public final boolean saveToTabRestoreService;
    public final @TabClosingSource int tabClosingSource;
    public final @TabCloseType int tabCloseType;
    public final @Nullable Runnable undoRunnable;
    public final boolean isTabGroup;

    private TabClosureParams(
            @Nullable List<Tab> tabs,
            boolean isAllTabs,
            @Nullable Tab recommendedNextTab,
            boolean uponExit,
            boolean allowUndo,
            boolean hideTabGroups,
            boolean saveToTabRestoreService,
            @TabClosingSource int tabClosingSource,
            @TabCloseType int tabCloseType,
            @Nullable Runnable undoRunnable,
            boolean isTabGroup) {
        this.tabs = tabs;
        this.isAllTabs = isAllTabs;
        this.recommendedNextTab = recommendedNextTab;
        this.uponExit = uponExit;
        this.allowUndo = allowUndo;
        this.hideTabGroups = hideTabGroups;
        this.saveToTabRestoreService = saveToTabRestoreService;
        this.tabClosingSource = tabClosingSource;
        this.tabCloseType = tabCloseType;
        this.undoRunnable = undoRunnable;
        this.isTabGroup = isTabGroup;
    }

    @Override
    public boolean equals(Object other) {
        if (this == other) return true;

        if (other instanceof TabClosureParams otherParams) {
            return Objects.equals(this.tabs, otherParams.tabs)
                    && this.isAllTabs == otherParams.isAllTabs
                    && Objects.equals(this.recommendedNextTab, otherParams.recommendedNextTab)
                    && this.uponExit == otherParams.uponExit
                    && this.allowUndo == otherParams.allowUndo
                    && this.hideTabGroups == otherParams.hideTabGroups
                    && this.saveToTabRestoreService == otherParams.saveToTabRestoreService
                    && this.tabClosingSource == otherParams.tabClosingSource
                    && this.tabCloseType == otherParams.tabCloseType
                    && Objects.equals(this.undoRunnable, otherParams.undoRunnable)
                    && this.isTabGroup == otherParams.isTabGroup;
        }
        return false;
    }

    @Override
    public int hashCode() {
        return Objects.hash(
                this.tabs,
                this.isAllTabs,
                this.recommendedNextTab,
                this.uponExit,
                this.allowUndo,
                this.hideTabGroups,
                this.saveToTabRestoreService,
                this.tabClosingSource,
                this.tabCloseType,
                this.undoRunnable,
                this.isTabGroup);
    }

    @Override
    public String toString() {
        return "tabs "
                + this.tabs
                + "\nisAllTabs "
                + this.isAllTabs
                + "\nrecommendedNextTab "
                + this.recommendedNextTab
                + "\nuponExit "
                + this.uponExit
                + "\nallowUndo "
                + this.allowUndo
                + "\nhideTabGroups "
                + this.hideTabGroups
                + "\nsaveToTabRestoreService "
                + this.saveToTabRestoreService
                + "\ntabClosingSource "
                + this.tabClosingSource
                + "\ntabCloseType "
                + this.tabCloseType
                + "\nundoRunnable "
                + this.undoRunnable
                + "\nisTabGroup "
                + this.isTabGroup;
    }
}
