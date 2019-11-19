// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.snackbar.undo;

import android.content.Context;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.device.DeviceClassManager;
import org.chromium.chrome.browser.snackbar.Snackbar;
import org.chromium.chrome.browser.snackbar.SnackbarManager;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.EmptyTabModelObserver;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelObserver;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.util.AccessibilityUtil;

import java.util.List;
import java.util.Locale;

/**
 * A controller that listens to and visually represents cancelable tab closures.
 * <p/>
 * Each time a tab is undoably closed via {@link TabModelObserver#tabPendingClosure(Tab)},
 * this controller saves that tab id and title to the stack of SnackbarManager. It will then let
 * SnackbarManager to show a snackbar representing the top entry in of stack. Each added entry
 * resets the timeout that tracks when to commit the undoable actions.
 * <p/>
 * When the undo button is clicked, it will cancel the tab closure if any. all pending closing will
 * be committed.
 * <p/>
 * This class also responds to external changes to the undo state by monitoring
 * {@link TabModelObserver#tabClosureUndone(Tab)} and
 * {@link TabModelObserver#tabClosureCommitted(Tab)} to properly keep it's internal state
 * in sync with the model.
 */
public class UndoBarController implements SnackbarManager.SnackbarController {
    // AndroidTabCloseUndoToastEvent defined in tools/metrics/histograms/histograms.xml.
    private static final int TAB_CLOSE_UNDO_TOAST_SHOWN_COLD = 0;
    private static final int TAB_CLOSE_UNDO_TOAST_SHOWN_WARM = 1;
    private static final int TAB_CLOSE_UNDO_TOAST_PRESSED = 2;
    private static final int TAB_CLOSE_UNDO_TOAST_COUNT = 5;

    private final TabModelSelector mTabModelSelector;
    private final TabModelObserver mTabModelObserver;
    private final SnackbarManager.SnackbarManageable mSnackbarManagable;
    private final Context mContext;

    /**
     * Creates an instance of a {@link UndoBarController}.
     * @param context The {@link Context} in which snackbar is shown.
     * @param selector The {@link TabModelSelector} that will be used to commit and undo tab
     *                 closures.
     * @param snackbarManagable The holder class to get the manager that helps to show up snackbar.
     */
    public UndoBarController(Context context, TabModelSelector selector,
            SnackbarManager.SnackbarManageable snackbarManagable) {
        mSnackbarManagable = snackbarManagable;
        mTabModelSelector = selector;
        mContext = context;
        mTabModelObserver = new EmptyTabModelObserver() {
            private boolean disableUndo() {
                return AccessibilityUtil.isAccessibilityEnabled()
                        || DeviceClassManager.enableAccessibilityLayout();
            }

            @Override
            public void tabPendingClosure(Tab tab) {
                if (disableUndo()) return;
                showUndoBar(tab.getId(), tab.getTitle());
            }

            @Override
            public void tabClosureUndone(Tab tab) {
                if (disableUndo()) return;
                mSnackbarManagable.getSnackbarManager().dismissSnackbars(
                        UndoBarController.this, tab.getId());
            }

            @Override
            public void tabClosureCommitted(Tab tab) {
                if (disableUndo()) return;
                mSnackbarManagable.getSnackbarManager().dismissSnackbars(
                        UndoBarController.this, tab.getId());
            }

            @Override
            public void multipleTabsPendingClosure(List<Tab> tabs, boolean isAllTabs) {
                if (disableUndo()) return;

                if (tabs.size() == 1) {
                    tabPendingClosure(tabs.get(0));
                    return;
                }

                // "Undo close all" bar can be reused for undoing close multiple tabs.
                showUndoCloseMultipleBar(tabs, isAllTabs);
            }

            @Override
            public void allTabsClosureCommitted() {
                if (disableUndo()) return;
                mSnackbarManagable.getSnackbarManager().dismissSnackbars(UndoBarController.this);
            }
        };
    }

    /**
     * Carry out native library dependent operations like registering observers and notifications.
     */
    public void initialize() {
        mTabModelSelector.getModel(false).addObserver(mTabModelObserver);
    }

    /**
     * Cleans up this class, unregistering for application notifications from the
     * {@link TabModelSelector}.
     */
    public void destroy() {
        TabModel model = mTabModelSelector.getModel(false);
        if (model != null) model.removeObserver(mTabModelObserver);
    }

    /**
     * Shows an undo bar. Based on user actions, this will cause a call to either
     * {@link TabModel#commitTabClosure(int)} or {@link TabModel#cancelTabClosure(int)} to be called
     * for {@code tabId}.
     *
     * @param tabId The id of the tab.
     * @param content The title of the tab.
     */
    private void showUndoBar(int tabId, String content) {
        RecordHistogram.recordEnumeratedHistogram("AndroidTabCloseUndo.Toast",
                mSnackbarManagable.getSnackbarManager().isShowing()
                        ? TAB_CLOSE_UNDO_TOAST_SHOWN_WARM
                        : TAB_CLOSE_UNDO_TOAST_SHOWN_COLD,
                TAB_CLOSE_UNDO_TOAST_COUNT);
        mSnackbarManagable.getSnackbarManager().showSnackbar(
                Snackbar.make(content, this, Snackbar.TYPE_ACTION, Snackbar.UMA_TAB_CLOSE_UNDO)
                        .setTemplateText(mContext.getString(R.string.undo_bar_close_message))
                        .setAction(mContext.getString(R.string.undo), tabId));
    }

    /**
     * Shows an undo close all bar. Based on user actions, this will cause a call to either
     * {@link TabModel#commitTabClosure(int)} or {@link TabModel#cancelTabClosure(int)} to be called
     * for each tab in {@code closedTabIds}. This will happen unless
     * {@code SnackbarManager#removeFromStackForData(Object)} is called.
     *
     * @param closedTabs A list of tabs that were closed.
     * @param isAllTabs Whether all tabs were closed.
     */
    private void showUndoCloseMultipleBar(List<Tab> closedTabs, boolean isAllTabs) {
        String content = String.format(Locale.getDefault(), "%d", closedTabs.size());
        mSnackbarManagable.getSnackbarManager().showSnackbar(
                Snackbar.make(content, this, Snackbar.TYPE_ACTION,
                                isAllTabs ? Snackbar.UMA_TAB_CLOSE_ALL_UNDO
                                          : Snackbar.UMA_TAB_CLOSE_MULTIPLE_UNDO)
                        .setTemplateText(mContext.getString(R.string.undo_bar_close_all_message))
                        .setAction(mContext.getString(R.string.undo), closedTabs));
    }

    /**
     * Calls {@link TabModel#cancelTabClosure(int)} for the tab or for each tab in
     * the list of closed tabs.
     */
    @SuppressWarnings("unchecked")
    @Override
    public void onAction(Object actionData) {
        RecordHistogram.recordEnumeratedHistogram("AndroidTabCloseUndo.Toast",
                TAB_CLOSE_UNDO_TOAST_PRESSED, TAB_CLOSE_UNDO_TOAST_COUNT);
        if (actionData instanceof Integer) {
            cancelTabClosure((Integer) actionData);
        } else {
            for (Tab tab : (List<Tab>) actionData) {
                cancelTabClosure(tab.getId());
            }
        }
    }

    private void cancelTabClosure(int tabId) {
        TabModel model = mTabModelSelector.getModelForTabId(tabId);
        if (model != null) model.cancelTabClosure(tabId);
    }

    /**
     * Calls {@link TabModel#commitTabClosure(int)} for the tab or for each tab in
     * the list of closed tabs.
     */
    @SuppressWarnings("unchecked")
    @Override
    public void onDismissNoAction(Object actionData) {
        if (actionData instanceof Integer) {
            commitTabClosure((Integer) actionData);
        } else {
            for (Tab tab : (List<Tab>) actionData) {
                commitTabClosure(tab.getId());
            }
        }
    }

    private void commitTabClosure(int tabId) {
        TabModel model = mTabModelSelector.getModelForTabId(tabId);
        if (model != null) model.commitTabClosure(tabId);
    }
}
