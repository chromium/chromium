// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.actions.glic;

import static org.chromium.chrome.browser.ui.actions.button.ButtonState.DEFAULT;
import static org.chromium.chrome.browser.ui.actions.button.ButtonState.UNCLICKABLE;

import android.app.Activity;
import android.view.View;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.base.supplier.NullableObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.actor.ActorTask;
import org.chromium.chrome.browser.browser_controls.BrowserControlsVisibilityManager;
import org.chromium.chrome.browser.glic.GlicButtonDelegate;
import org.chromium.chrome.browser.glic.GlicButtonStateController;
import org.chromium.chrome.browser.glic.GlicTaskMenuCoordinator;
import org.chromium.chrome.browser.tab.CurrentTabObserver;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.ui.actions.ActionId;
import org.chromium.chrome.browser.ui.actions.ActionProperties;
import org.chromium.chrome.browser.ui.actions.ActionRegistry;
import org.chromium.chrome.browser.ui.browser_window.ChromeAndroidTask;
import org.chromium.components.embedder_support.util.UrlUtilities;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.List;
import java.util.function.Supplier;

/** Coordinator for the Glic action. */
@NullMarked
public class GlicActionCoordinator {
    private final NullableObservableSupplier<PropertyModel> mGlicActionModelSupplier;
    private final Callback<@Nullable PropertyModel> mModelCallback = this::onModelChanged;
    private final GlicButtonDelegate mToggleGlicCallback;
    private final NullableObservableSupplier<Tab> mTabSupplier;
    private final CurrentTabObserver mCurrentTabObserver;
    private final GlicButtonStateController mStateController;
    private final Supplier<@Nullable TabModelSelector> mTabModelSelectorSupplier;
    private @Nullable GlicTaskMenuCoordinator mTaskMenuCoordinator;

    public GlicActionCoordinator(
            Activity activity,
            ActionRegistry actionRegistry,
            GlicButtonDelegate toggleGlicCallback,
            NullableObservableSupplier<Tab> tabSupplier,
            Supplier<@Nullable ChromeAndroidTask> taskSupplier,
            BrowserControlsVisibilityManager browserControlsVisibilityManager,
            Supplier<@Nullable TabModelSelector> tabModelSelectorSupplier) {
        mToggleGlicCallback = toggleGlicCallback;
        mTabSupplier = tabSupplier;
        mTabModelSelectorSupplier = tabModelSelectorSupplier;
        mGlicActionModelSupplier = actionRegistry.get(ActionId.GLIC);

        mStateController =
                new GlicButtonStateController(
                        activity,
                        this::onStateChanged,
                        taskSupplier,
                        browserControlsVisibilityManager);

        if (mGlicActionModelSupplier != null) {
            mGlicActionModelSupplier.addSyncObserverAndCallIfNonNull(mModelCallback);
        }

        mCurrentTabObserver =
                new CurrentTabObserver(
                        tabSupplier,
                        new EmptyTabObserver() {
                            @Override
                            public void onUrlUpdated(Tab tab) {
                                updateButtonState();
                            }
                        },
                        (tab) -> {
                            updateButtonState();
                            if (tab == null || tab.isOffTheRecord()) return;

                            mStateController.updateObservations(tab.getProfile());
                            mStateController.updateButtonState();
                        });
    }

    private void onModelChanged(@Nullable PropertyModel model) {
        if (model == null) return;
        model.set(ActionProperties.ON_PRESS_CALLBACK, this::onGlicActionPressed);
        updateButtonState();
    }

    @VisibleForTesting
    /* package */ void onStateChanged(
            @GlicButtonStateController.ButtonState int state, boolean isPanelOpen) {
        PropertyModel model =
                mGlicActionModelSupplier != null ? mGlicActionModelSupplier.get() : null;
        if (model != null) {
            model.set(GlicActionProperties.GLIC_STATE, state);
        }
    }

    private void onGlicActionPressed(View view) {
        Tab currentTab = mTabSupplier.get();
        List<ActorTask> tasks = mStateController.getActiveTasks();

        boolean isOnActingTab =
                currentTab != null
                        && mStateController.getActiveTaskIdOnTab(currentTab.getId()) != null;

        // If there are no tasks, or we are already on the tab with the active task, just toggle
        // Glic.
        if (tasks == null || tasks.isEmpty() || isOnActingTab) {
            mToggleGlicCallback.onClick(false);
            mStateController.updateButtonState();
            return;
        }

        // Otherwise, show the task menu.
        if (mTaskMenuCoordinator == null) {
            mTaskMenuCoordinator =
                    new GlicTaskMenuCoordinator(
                            view.getContext(), mTabModelSelectorSupplier, mToggleGlicCallback);
        }
        mTaskMenuCoordinator.show(view, tasks);
    }

    private void updateButtonState() {
        PropertyModel model =
                mGlicActionModelSupplier != null ? mGlicActionModelSupplier.get() : null;
        if (model == null) return;

        Tab currentTab = mTabSupplier.get();
        boolean isEnabled =
                currentTab != null
                        && !currentTab.isOffTheRecord()
                        && !UrlUtilities.isNtpUrl(currentTab.getUrl());

        model.set(ActionProperties.BUTTON_STATE, isEnabled ? DEFAULT : UNCLICKABLE);
    }

    public void destroy() {
        if (mGlicActionModelSupplier != null) {
            mGlicActionModelSupplier.removeObserver(mModelCallback);
        }
        mCurrentTabObserver.destroy();
        mStateController.destroy();
        if (mTaskMenuCoordinator != null) {
            mTaskMenuCoordinator.dismiss();
        }
    }
}
