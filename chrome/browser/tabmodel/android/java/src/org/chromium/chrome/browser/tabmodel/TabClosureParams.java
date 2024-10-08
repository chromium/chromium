// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import androidx.annotation.Nullable;

import com.google.errorprone.annotations.DoNotMock;

import org.chromium.chrome.browser.tab.Tab;

import java.util.Arrays;
import java.util.List;
import java.util.Objects;

/** Parameters to control closing tabs from the {@link TabModel}. */
@DoNotMock("Create a real instance instead.")
public class TabClosureParams {
    /**
     * Returns a new {@link TabClosureParams.CloseTabBuilder} to instantiate {@link
     * TabClosureParams}.
     */
    public static TabClosureParams.CloseTabBuilder closeTab(Tab tab) {
        return new TabClosureParams.CloseTabBuilder(tab);
    }

    /**
     * Returns a new {@link TabClosureParams.CloseTabsBuilder} to instantiate {@link
     * TabClosureParams}.
     */
    public static TabClosureParams.CloseTabsBuilder closeTabs(List<Tab> tabs) {
        return new TabClosureParams.CloseTabsBuilder(tabs);
    }

    /**
     * Returns a new {@link TabClosureParams.CloseAllTabsBuilder} to instantiate {@link
     * TabClosureParams}.
     */
    public static TabClosureParams.CloseAllTabsBuilder closeAllTabs() {
        return new TabClosureParams.CloseAllTabsBuilder();
    }

    /** Builder to configure params for closing a single tab. */
    public static class CloseTabBuilder {
        private final Tab mTab;
        private boolean mAllowUndo = true;
        private boolean mUponExit;
        private @Nullable Tab mRecommendedNextTab;

        private CloseTabBuilder(Tab tab) {
            mTab = tab;
        }

        /** Sets the recommended next tab to select. Default is null. */
        public CloseTabBuilder recommendedNextTab(@Nullable Tab recommendedNextTab) {
            mRecommendedNextTab = recommendedNextTab;
            return this;
        }

        /** Sets whether the tab closure completing would exit the app. Default is false. */
        public CloseTabBuilder uponExit(boolean uponExit) {
            mUponExit = uponExit;
            return this;
        }

        /** Set whether to allow undo. Default is true. */
        public CloseTabBuilder allowUndo(boolean allowUndo) {
            mAllowUndo = allowUndo;
            return this;
        }

        /** Builds the params. */
        public TabClosureParams build() {
            return new TabClosureParams(
                    Arrays.asList(mTab),
                    /* isAllTabs= */ false,
                    mRecommendedNextTab,
                    mUponExit,
                    mAllowUndo,
                    /* hideTabGroups= */ false,
                    /* saveToTabRestoreService= */ true,
                    TabCloseType.SINGLE,
                    /* undoRunnable= */ null);
        }
    }

    /** Builder to configure params for closing multiple tabs. */
    public static class CloseTabsBuilder {
        private final List<Tab> mTabs;
        private boolean mAllowUndo = true;
        private boolean mHideTabGroups;
        private boolean mSaveToTabRestoreService = true;

        private CloseTabsBuilder(List<Tab> tabs) {
            mTabs = tabs;
        }

        /** Set whether to allow undo. Default is true. */
        public CloseTabsBuilder allowUndo(boolean allowUndo) {
            mAllowUndo = allowUndo;
            return this;
        }

        /** Set whether to hide or delete tab groups. Default is delete. */
        public CloseTabsBuilder hideTabGroups(boolean hideTabGroups) {
            mHideTabGroups = hideTabGroups;
            return this;
        }

        /** Set whether to allow saving to the Tab Restore Service. Default is true. */
        public CloseTabsBuilder saveToTabRestoreService(boolean saveToTabRestoreService) {
            mSaveToTabRestoreService = saveToTabRestoreService;
            return this;
        }

        /** Builds the params. */
        public TabClosureParams build() {
            return new TabClosureParams(
                    mTabs,
                    /* isAllTabs= */ false,
                    /* recommendedNextTab= */ null,
                    /* uponExit= */ false,
                    mAllowUndo,
                    mHideTabGroups,
                    mSaveToTabRestoreService,
                    TabCloseType.MULTIPLE,
                    /* undoRunnable= */ null);
        }
    }

    /**
     * Builder to configure params for closing all tabs. Closing all tabs always allows for undo if
     * permitted by the tab model.
     */
    public static class CloseAllTabsBuilder {
        private boolean mUponExit;
        private boolean mHideTabGroups;
        private Runnable mUndoRunnable;

        private CloseAllTabsBuilder() {}

        /** Sets whether the tab closure completing would exit the app. Default is false. */
        public CloseAllTabsBuilder uponExit(boolean uponExit) {
            mUponExit = uponExit;
            return this;
        }

        /** Set whether to hide or delete tab groups. Default is delete. */
        public CloseAllTabsBuilder hideTabGroups(boolean hideTabGroups) {
            mHideTabGroups = hideTabGroups;
            return this;
        }

        /** Sets the undo runnable. */
        public CloseAllTabsBuilder withUndoRunnable(Runnable undoRunnable) {
            mUndoRunnable = undoRunnable;
            return this;
        }

        /** Builds the params. */
        public TabClosureParams build() {
            return new TabClosureParams(
                    /* tabs= */ null,
                    /* isAllTabs= */ true,
                    /* recommendedNextTab= */ null,
                    mUponExit,
                    /* allowUndo= */ true,
                    mHideTabGroups,
                    /* saveToTabRestoreService= */ true,
                    TabCloseType.ALL,
                    /* undoRunnable= */ mUndoRunnable);
        }
    }

    // TODO(crbug.com/356445932): Consider package protecting these fields once TabGroupModelFilter
    // is merged into TabModel.
    public final @Nullable List<Tab> tabs;
    public final boolean isAllTabs;
    public final @Nullable Tab recommendedNextTab;
    public final boolean uponExit;
    public final boolean allowUndo;
    public final boolean hideTabGroups;
    public final boolean saveToTabRestoreService;
    public final @TabCloseType int tabCloseType;
    public final Runnable undoRunnable;

    private TabClosureParams(
            @Nullable List<Tab> tabs,
            boolean isAllTabs,
            @Nullable Tab recommendedNextTab,
            boolean uponExit,
            boolean allowUndo,
            boolean hideTabGroups,
            boolean saveToTabRestoreService,
            @TabCloseType int tabCloseType,
            @Nullable Runnable undoRunnable) {
        this.tabs = tabs;
        this.isAllTabs = isAllTabs;
        this.recommendedNextTab = recommendedNextTab;
        this.uponExit = uponExit;
        this.allowUndo = allowUndo;
        this.hideTabGroups = hideTabGroups;
        this.saveToTabRestoreService = saveToTabRestoreService;
        this.tabCloseType = tabCloseType;
        this.undoRunnable = undoRunnable;
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
                    && this.tabCloseType == otherParams.tabCloseType
                    && Objects.equals(this.undoRunnable, otherParams.undoRunnable);
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
                this.tabCloseType,
                this.undoRunnable);
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
                + "\ntabCloseType "
                + this.tabCloseType
                + "\nundoRunnable "
                + this.undoRunnable;
    }
}
