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
        private boolean mAllowUnloadHandlers = true;

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
         * Sets whether the tabs being closed may run their {@code beforeunload} and {@code unload}
         * handlers before they are destroyed. Default is true.
         *
         * <p>True does not mean a handler will run: the closure still has to reach a call site
         * that consults this, the capability has to be enabled for this build and form factor,
         * the page has to have registered a handler, and on {@code beforeunload} the user still
         * has to confirm. False means the tabs are destroyed without the handlers being given a
         * chance, and a registered {@code beforeunload} handler cannot cancel the closure.
         *
         * @see TabClosureParamsUtils#areUnloadHandlersEnabled()
         */
        public Builder allowUnloadHandlers(boolean allowUnloadHandlers) {
            mAllowUnloadHandlers = allowUnloadHandlers;
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

        /**
         * Copies every field of {@code params} that this builder's close type can carry. The tabs,
         * the close type, and whether this is an all-tabs closure are excluded: they identify the
         * closure and are fixed by the factory that produced this builder.
         *
         * <p>Each restricted field is routed through its setter behind the matching {@code canSet*}
         * predicate, so the copy re-runs the availability matrix rather than duplicating it. A
         * field the destination close type cannot carry is skipped, and skipping loses nothing:
         * {@link TabClosureParams}'s constructor is private, so {@code params} was itself built
         * through a {@link Builder} and already satisfies the matrix, which means the value being
         * skipped is necessarily the default.
         *
         * <p><b>Must be called on a builder whose fields are still at their defaults.</b> The
         * {@code canSet*} predicates compare against the builder's current value, so on an
         * already-configured builder they would answer "this write is a no-op" for a value that had
         * been written rather than defaulted, and the copy would skip a field it should carry. Both
         * call sites pass a freshly constructed builder.
         *
         * <p>Adding a field to {@link TabClosureParams} requires adding it in <b>three</b> places
         * for the round-trip tests to catch a mistake: here, in {@link #equals}, and in a test that
         * sets it to a non-default value. Miss any one of the three and the suite stays green while
         * the field is silently dropped.
         */
        private Builder copyCarriedFieldsFrom(TabClosureParams params) {
            if (canSetRecommendedNextTab(params.recommendedNextTab)) {
                recommendedNextTab(params.recommendedNextTab);
            }
            if (canSetUponExit(params.uponExit)) uponExit(params.uponExit);
            allowUndo(params.allowUndo);
            if (canSetHideTabGroups(params.hideTabGroups)) hideTabGroups(params.hideTabGroups);
            if (canSetSaveToTabRestoreService(params.saveToTabRestoreService)) {
                saveToTabRestoreService(params.saveToTabRestoreService);
            }
            tabClosingSource(params.tabClosingSource);
            withUndoRunnable(params.undoRunnable);
            if (canSetIsTabGroup(params.isTabGroup)) isTabGroup(params.isTabGroup);
            allowUnloadHandlers(params.allowUnloadHandlers);
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
                    mIsTabGroup,
                    mAllowUnloadHandlers);
        }
    }

    // TODO(crbug.com/356445932): Consider package protecting these fields.
    // TODO(crbug.com/356445932): `tabs` is stored by reference, so a closure, the list its
    // caller passed in, and any builder derived from it all share one List. A
    // `new ArrayList<>(tabs)` in the Builder constructor would give each closure its own
    // copy; do it with the field lockdown above, since both change what callers may do
    // with these fields.
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

    /**
     * Whether the tabs being closed may run their {@code beforeunload} and {@code unload} handlers
     * before they are destroyed. See {@link Builder#allowUnloadHandlers}.
     */
    public final boolean allowUnloadHandlers;

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
            boolean isTabGroup,
            boolean allowUnloadHandlers) {
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
        this.allowUnloadHandlers = allowUnloadHandlers;
    }

    /**
     * Returns a {@link Builder} seeded with this closure, for deriving a variation of it.
     *
     * <p>The tabs, the close type, and {@code isAllTabs} carry over and cannot be changed: a copy
     * may alter what a closure does, never what kind of closure it is or which tabs it acts on.
     * Retargeting the same kind of closure at a different tab list requires {@link
     * #toBuilder(List)}; deriving a different kind requires an explicit factory call. Both keep the
     * change visible at the call site.
     *
     * <p>The tab list is shared by reference with this closure rather than copied. See the second
     * TODO on the fields above.
     */
    public Builder toBuilder() {
        return new Builder(tabCloseType, isAllTabs, tabs).copyCarriedFieldsFrom(this);
    }

    /**
     * As {@link #toBuilder()}, but retargets the closure at {@code tabs}.
     *
     * <p>Unlike the factories, which accept an empty list because some callers legitimately build a
     * closure over no tabs, a <em>replacement</em> list may not be empty: dropping every tab means
     * the closure should not be derived at all, and the caller is expected to have handled that.
     */
    public Builder toBuilder(List<Tab> tabs) {
        assert !isAllTabs : "An all-tabs closure does not carry an explicit tab list to replace.";
        assert !tabs.isEmpty() : "The replacement tab list must not be empty.";
        assert tabCloseType != TabCloseType.SINGLE || tabs.size() == 1
                : "Closing a single tab requires exactly one tab.";
        return new Builder(tabCloseType, isAllTabs, tabs).copyCarriedFieldsFrom(this);
    }

    /**
     * Converts this all-tabs closure into a {@link Builder} for a multi-tab closure over {@code
     * tabs}, for the case where some tabs must be spared and the closure is therefore no longer a
     * close-all.
     *
     * <p>This is the one sanctioned change of closure kind, and it is a narrowing: every field a
     * multi-tab closure can carry is carried over, and the rest are dropped by {@link
     * Builder#copyCarriedFieldsFrom} because {@code MULTIPLE} does not support them.
     *
     * <p>As with {@link #toBuilder(List)}, the surviving tab list may not be empty.
     */
    public Builder toPartialClosureBuilder(List<Tab> tabs) {
        assert tabCloseType == TabCloseType.ALL
                : "Only an all-tabs closure can be narrowed to a partial closure.";
        assert !tabs.isEmpty() : "The surviving tab list must not be empty.";
        return closeTabs(tabs).copyCarriedFieldsFrom(this);
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
                    && this.isTabGroup == otherParams.isTabGroup
                    && this.allowUnloadHandlers == otherParams.allowUnloadHandlers;
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
                this.isTabGroup,
                this.allowUnloadHandlers);
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
                + this.isTabGroup
                + "\nallowUnloadHandlers "
                + this.allowUnloadHandlers;
    }
}
