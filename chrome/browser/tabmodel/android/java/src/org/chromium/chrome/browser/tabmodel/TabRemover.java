// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import org.chromium.base.Callback;
import org.chromium.base.UserDataHost;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModelActionListener.DialogType;
import org.chromium.chrome.browser.ui.native_page.BeforeUnloadCallback;
import org.chromium.components.browser_ui.widget.ActionConfirmationResult;

import java.util.ArrayList;
import java.util.List;

/**
 * Handles removal of {@link Tab} entities from a {@link TabModel} via closure or removal. Also
 * handles the ungrouping of tabs for {@link TabModel}. This interface is intended to be a
 * counterpart of {@link TabCreator}.
 *
 * <p>This interface, combined with {@link TabUngrouper}, facilitates a shared implementation with
 * the ability to show warning dialogs when events may be destructive to tab groups.
 */
@NullMarked
public interface TabRemover {
    /**
     * Asks a tab whether it may close, for reasons {@link #checkBeforeUnloadAndProceed} cannot ask
     * about itself.
     *
     * <p>An implementation decides per tab whether it has anything to ask, and reports that through
     * its return value.
     */
    interface TabClosePrompter {
        /**
         * Asks {@code tab} whether it may close.
         *
         * @param tabClosureParams The closure {@code tab} belongs to.
         * @param tab The tab being asked.
         * @param onProceed Run once if the tab agrees to close.
         * @param onCancel Run once if the tab refuses.
         * @return Whether a prompt is now pending. When true, exactly one of {@code onProceed} and
         *     {@code onCancel} runs later and the closure must not advance until it does. When
         *     false, neither has run.
         */
        boolean prompt(
                TabClosureParams tabClosureParams, Tab tab, Runnable onProceed, Runnable onCancel);
    }

    /**
     * Closes tabs based on the provided parameters. Refer to {@link TabClosureParams} for different
     * ways to close tabs.
     *
     * @param tabClosureParams The parameters to follow when closing tabs.
     * @param allowDialog Whether the operation is allowed to show a dialog if it is determined that
     *     the operation is destructive to a tab group. Prefer to pass true here unless there is
     *     reason to believe the action is not user visible or not user controllable.
     * @param listener A {@link TabModelActionListener} that receives updates about the closure
     *     process.
     */
    void closeTabs(
            TabClosureParams tabClosureParams,
            boolean allowDialog,
            @Nullable TabModelActionListener listener);

    /**
     * {@link #closeTabs(TabClosureParams, boolean, TabModelActionListener)} without the {@code
     * listener} or {@code performActionOverride}.
     */
    default void closeTabs(TabClosureParams tabClosureParams, boolean allowDialog) {
        closeTabs(tabClosureParams, allowDialog, /* listener= */ null);
    }

    /**
     * Prepares to close tabs based on the provided parameters. This is similar to {@link
     * closeTabs}. However, it doesn't close the tabs. Instead the final {@link TabClosureParams}
     * are supplied to the {@code onPreparedCallback}. This allows a caller to then perform
     * additional actions before committing to close the tabs with {@link forceCloseTabs} or {@link
     * closeTabs} with {@code allowDialog = false}.
     *
     * @param tabClosureParams The parameters to follow when closing tabs.
     * @param allowDialog Whether the operation is allowed to show a dialog if it is determined that
     *     the operation is destructive to a tab group. Prefer to pass true here unless there is
     *     reason to believe the action is not user visible or not user controllable.
     * @param listener A {@link TabModelActionListener} that receives updates about the closure
     *     process.
     * @param onPreparedCallback A callback invoked with {@code tabClosureParams} that should be
     *     used to close the tabs.
     */
    void prepareCloseTabs(
            TabClosureParams tabClosureParams,
            boolean allowDialog,
            @Nullable TabModelActionListener listener,
            Callback<TabClosureParams> onPreparedCallback);

    /** Closes tabs bypassing any dialogs and data sharing protections. */
    void forceCloseTabs(TabClosureParams tabClosureParams);

    /**
     * Removes the given tab from the model without destroying it. The tab should be inserted into
     * another model to avoid leaking as after this the link to the old Activity will be broken.
     *
     * @param tab The tab to remove.
     * @param allowDialog Whether the operation is allowed to show a dialog if it is determined that
     *     the operation is destructive to a tab group. Prefer to pass true here unless there is
     *     reason to believe the action is not user visible or not user controllable.
     * @param listener A {@link TabModelActionListener} that receives updates about the closure
     *     process.
     */
    void removeTab(Tab tab, boolean allowDialog, @Nullable TabModelActionListener listener);

    /** {@link #removeTab(Tab, boolean, TabModelActionListener)} without the {@code listener}. */
    default void removeTab(Tab tab, boolean allowDialog) {
        removeTab(tab, allowDialog, /* listener= */ null);
    }

    /**
     * Asks each tab in turn whether it may close, then supplies the tabs that agreed.
     *
     * <p>Two mechanisms can ask: a tab's {@link BeforeUnloadCallback}, which native pages such as
     * PDF register, and {@code extraPrompter}. A tab that neither claims agrees by default. The
     * walk is sequential, so at most one prompt is on screen at a time.
     *
     * @param tabClosureParams The closure being performed. Forwarded to {@code extraPrompter}.
     * @param tabs The tabs to ask.
     * @param extraPrompter A prompt to try for each tab, or null to use only {@link
     *     BeforeUnloadCallback}.
     * @param abandonBatchOnCancel Whether one refusal abandons the whole closure. A caller that
     *     cannot express a partial closure passes true.
     * @param listener A {@link TabModelActionListener} that receives updates about the closure.
     * @param onConfirmed Supplied with the tabs that agreed to close, in their original order. Not
     *     run at all when a refusal abandons the closure.
     */
    static void checkBeforeUnloadAndProceed(
            TabClosureParams tabClosureParams,
            List<Tab> tabs,
            @Nullable TabClosePrompter extraPrompter,
            boolean abandonBatchOnCancel,
            @Nullable TabModelActionListener listener,
            Callback<List<Tab>> onConfirmed) {
        checkBeforeUnloadAndProceed(
                tabClosureParams,
                tabs,
                extraPrompter,
                abandonBatchOnCancel,
                listener,
                onConfirmed,
                /* index= */ 0,
                new ArrayList<>());
    }

    /**
     * Asks the tabs from {@code index} onwards, carrying the tabs that have already agreed in
     * {@code confirmedTabs}. A tab that shows a prompt suspends the walk and resumes it from the
     * callback it was given.
     */
    private static void checkBeforeUnloadAndProceed(
            TabClosureParams tabClosureParams,
            List<Tab> tabs,
            @Nullable TabClosePrompter extraPrompter,
            boolean abandonBatchOnCancel,
            @Nullable TabModelActionListener listener,
            Callback<List<Tab>> onConfirmed,
            int index,
            List<Tab> confirmedTabs) {
        for (int i = index; i < tabs.size(); i++) {
            Tab tab = tabs.get(i);
            int nextIndex = i + 1;
            Runnable onResume =
                    () ->
                            checkBeforeUnloadAndProceed(
                                    tabClosureParams,
                                    tabs,
                                    extraPrompter,
                                    abandonBatchOnCancel,
                                    listener,
                                    onConfirmed,
                                    nextIndex,
                                    confirmedTabs);
            Runnable onProceed =
                    () -> {
                        confirmedTabs.add(tab);
                        onResume.run();
                    };
            Runnable onCancel =
                    () -> {
                        if (!abandonBatchOnCancel) {
                            onResume.run();
                            return;
                        }
                        if (listener != null) {
                            listener.willPerformActionOrShowDialog(
                                    DialogType.NONE, /* willSkipDialog= */ false);
                            listener.onConfirmationDialogResult(
                                    DialogType.NONE,
                                    ActionConfirmationResult.CONFIRMATION_NEGATIVE);
                        }
                    };

            // A mechanism that reports no pending prompt has not asked, so it must not have
            // answered either. One that answers anyway resumes the walk from inside this frame,
            // and the tab is then confirmed twice and the rest of the closure walked twice. The
            // count below catches the common form of that mistake.
            int confirmedCount = confirmedTabs.size();

            UserDataHost userDataHost = tab.isDestroyed() ? null : tab.getUserDataHost();
            BeforeUnloadCallback callback =
                    userDataHost != null
                            ? userDataHost.getUserData(BeforeUnloadCallback.class)
                            : null;
            if (callback != null && callback.handleBeforeUnload(onProceed, onCancel)) {
                // A prompt is pending. It resumes the walk through onProceed or onCancel.
                return;
            }
            assert confirmedTabs.size() == confirmedCount
                    : "BeforeUnloadCallback answered without reporting a pending prompt.";

            if (extraPrompter != null
                    && extraPrompter.prompt(tabClosureParams, tab, onProceed, onCancel)) {
                // A prompt is pending. It resumes the walk through onProceed or onCancel.
                return;
            }
            assert confirmedTabs.size() == confirmedCount
                    : "TabClosePrompter answered without reporting a pending prompt.";

            confirmedTabs.add(tab);
        }

        onConfirmed.onResult(confirmedTabs);
    }

    /**
     * Turns the tabs that agreed to close into the closure to perform, and supplies it to {@code
     * onConfirmed} as a single closure so that undo, tab group bookkeeping and the tab restore
     * service see one event however many prompts it took.
     *
     * <p>Nothing is supplied when no tab is left to close; the listener hears a negative result
     * instead.
     *
     * @param tabClosureParams The closure the caller asked for.
     * @param tabModel The model the tabs belong to.
     * @param tabsAsked The tabs the closure covered before any of them were asked.
     * @param confirmedTabs The subset of {@code tabsAsked} that agreed to close.
     * @param listener A {@link TabModelActionListener} that receives updates about the closure.
     * @param onConfirmed Supplied with the closure to perform.
     */
    static void proceedWithConfirmedTabs(
            TabClosureParams tabClosureParams,
            TabModel tabModel,
            List<Tab> tabsAsked,
            List<Tab> confirmedTabs,
            @Nullable TabModelActionListener listener,
            Callback<TabClosureParams> onConfirmed) {
        if (tabClosureParams.isAllTabs) {
            // A close-all abandons the batch on the first refusal, so every tab agreed to get
            // here. It names no tabs either, so a tab that left the model while the others were
            // being asked needs no fixing up.
            onConfirmed.onResult(tabClosureParams);
            return;
        }

        // A tab can leave the model while an earlier tab is being asked -- window.close(), a
        // closure started elsewhere -- and TabModel asserts on closing a tab it no longer holds.
        List<Tab> tabsToClose =
                TabModelUtils.getTabsById(
                        TabModelUtils.getTabIds(confirmedTabs),
                        tabModel,
                        /* allowClosing= */ false);

        if (tabsToClose.isEmpty()) {
            if (listener != null) {
                listener.willPerformActionOrShowDialog(
                        DialogType.NONE, /* willSkipDialog= */ false);
                listener.onConfirmationDialogResult(
                        DialogType.NONE, ActionConfirmationResult.CONFIRMATION_NEGATIVE);
            }
            return;
        }

        if (tabsToClose.size() == tabsAsked.size()) {
            // Nothing was spared and nothing was lost, so this is the closure the caller asked
            // for, object and all.
            onConfirmed.onResult(tabClosureParams);
            return;
        }

        // Only a multi-tab closure reaches here: a single-tab closure's list is either its one tab
        // or empty, and both are handled above. toBuilder() asserts the same thing from the far
        // side.
        onConfirmed.onResult(tabClosureParams.toBuilder(tabsToClose).build());
    }
}
