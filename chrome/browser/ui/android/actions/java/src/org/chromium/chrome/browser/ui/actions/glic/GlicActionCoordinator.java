// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.actions.glic;

import static org.chromium.chrome.browser.ui.actions.button.ButtonState.DEFAULT;
import static org.chromium.chrome.browser.ui.actions.button.ButtonState.UNCLICKABLE;

import android.app.Activity;
import android.graphics.drawable.Drawable;
import android.view.View;

import androidx.annotation.VisibleForTesting;
import androidx.appcompat.content.res.AppCompatResources;

import org.chromium.base.Callback;
import org.chromium.base.supplier.NullableObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.actor.ActorTask;
import org.chromium.chrome.browser.browser_controls.BrowserControlsVisibilityManager;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.glic.GlicButtonDelegate;
import org.chromium.chrome.browser.glic.GlicButtonStateController;
import org.chromium.chrome.browser.glic.GlicKeyedService.GlicInvocationSource;
import org.chromium.chrome.browser.glic.GlicTaskMenuCoordinator;
import org.chromium.chrome.browser.glic.GlicUiHelper;
import org.chromium.chrome.browser.tab.CurrentTabObserver;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.ui.actions.ActionId;
import org.chromium.chrome.browser.ui.actions.ActionProperties;
import org.chromium.chrome.browser.ui.actions.ActionRegistry;
import org.chromium.chrome.browser.ui.actions.R;
import org.chromium.chrome.browser.ui.bottombar.BottomBarConfigUtils;
import org.chromium.chrome.browser.ui.browser_window.ChromeAndroidTask;
import org.chromium.chrome.browser.ui.messages.snackbar.Snackbar;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.components.feature_engagement.EventConstants;
import org.chromium.ui.drawable.DirtyDotDrawableWrapper;
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
    private final Activity mActivity;
    private final SnackbarManager mSnackbarManager;
    private @Nullable GlicTaskMenuCoordinator mTaskMenuCoordinator;
    private final Drawable mDefaultDrawable;
    private final Drawable mFilledDrawable;
    private final Drawable mWorkingDrawable;
    private final Drawable mReviewDrawable;
    private final Drawable mDoneDrawable;

    public GlicActionCoordinator(
            Activity activity,
            ActionRegistry actionRegistry,
            GlicButtonDelegate toggleGlicCallback,
            NullableObservableSupplier<Tab> tabSupplier,
            Supplier<@Nullable ChromeAndroidTask> taskSupplier,
            BrowserControlsVisibilityManager browserControlsVisibilityManager,
            Supplier<@Nullable TabModelSelector> tabModelSelectorSupplier,
            SnackbarManager snackbarManager) {
        mActivity = activity;
        mToggleGlicCallback = toggleGlicCallback;
        mTabSupplier = tabSupplier;
        mTabModelSelectorSupplier = tabModelSelectorSupplier;
        mSnackbarManager = snackbarManager;
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

                            onStateChanged(
                                    mStateController.getButtonState(),
                                    mStateController.isPanelOpen());
                        });

        int glicIconResId =
                BottomBarConfigUtils.alwaysUseFilledIcon()
                        ? R.drawable.ic_spark_filled_24dp
                        : R.drawable.ic_spark_outlined_24dp;

        mDefaultDrawable = AppCompatResources.getDrawable(activity, glicIconResId);
        mFilledDrawable = AppCompatResources.getDrawable(activity, R.drawable.ic_spark_filled_24dp);
        mWorkingDrawable = GlicUiHelper.createWorkingDrawable(activity, mFilledDrawable);
        Drawable sparkIcon =
                AppCompatResources.getDrawable(activity, R.drawable.ic_spark_filled_24dp);
        int dotColor = activity.getColor(R.color.default_icon_color_accent1_baseline);
        int dotSize = activity.getResources().getDimensionPixelSize(R.dimen.glic_dirty_dot_size);
        Drawable dirtyDotFilledSpark = new DirtyDotDrawableWrapper(sparkIcon, dotColor, dotSize);
        mReviewDrawable = dirtyDotFilledSpark;
        mDoneDrawable = dirtyDotFilledSpark;
    }

    private void onModelChanged(@Nullable PropertyModel model) {
        if (model == null) return;
        model.set(ActionProperties.ON_PRESS_CALLBACK, this::onGlicActionPressed);
        updateButtonState();
    }

    @VisibleForTesting
    /* package */ void onStateChanged(
            @GlicButtonStateController.ButtonState int state, boolean isPanelOpen) {
        PropertyModel model = getModelSupplierOrNull();
        if (model != null) {
            Drawable desiredDrawable;
            switch (state) {
                case GlicButtonStateController.ButtonState.WORKING:
                    desiredDrawable = mWorkingDrawable;
                    break;
                case GlicButtonStateController.ButtonState.NEEDS_REVIEW:
                    desiredDrawable = mReviewDrawable;
                    break;
                case GlicButtonStateController.ButtonState.DONE:
                    desiredDrawable = mDoneDrawable;
                    break;
                case GlicButtonStateController.ButtonState.DEFAULT:
                default:
                    desiredDrawable = isPanelOpen ? mFilledDrawable : mDefaultDrawable;
                    break;
            }
            model.set(GlicActionProperties.GLIC_DRAWABLE, desiredDrawable);
            model.set(ActionProperties.IS_SELECTED, isPanelOpen);
        }
    }

    private void onGlicActionPressed(View view) {
        mStateController.setPersistDoneState(false);
        Tab currentTab = mTabSupplier.get();
        if (currentTab != null) {
            TrackerFactory.getTrackerForProfile(currentTab.getProfile())
                    .notifyEvent(EventConstants.ANDROID_BOTTOM_BAR_GLIC_USED);
        }
        List<ActorTask> tasks = mStateController.getActiveTasks();

        boolean isOnActingTab =
                currentTab != null
                        && mStateController.getActiveTaskIdOnTab(currentTab.getId()) != null;

        // If there are no tasks, or we are already on the tab with the active task, just toggle
        // Glic.
        if (tasks == null || tasks.isEmpty() || isOnActingTab) {
            mToggleGlicCallback.onClick(false, GlicInvocationSource.TOOLBAR_BUTTON);
            mStateController.updateButtonState();
            return;
        }

        // Otherwise, show the task menu.
        if (mTaskMenuCoordinator == null) {
            mTaskMenuCoordinator =
                    new GlicTaskMenuCoordinator(
                            view.getContext(),
                            mTabModelSelectorSupplier,
                            mToggleGlicCallback,
                            GlicInvocationSource.TOOLBAR_BUTTON);
        }
        mTaskMenuCoordinator.show(view, tasks);
    }

    private void updateButtonState() {
        PropertyModel model = getModelSupplierOrNull();
        if (model == null) return;

        Tab currentTab = mTabSupplier.get();
        boolean isIncognito = currentTab != null && currentTab.isOffTheRecord();

        if (isIncognito) {
            model.set(ActionProperties.ON_PRESS_CALLBACK, this::showIncognitoSnackbar);
            model.set(ActionProperties.BUTTON_STATE, DEFAULT);
            model.set(
                    ActionProperties.ICON_TINT,
                    GlicActionUtils.getIncognitoDisabledTint(mActivity));
        } else {
            model.set(ActionProperties.ON_PRESS_CALLBACK, this::onGlicActionPressed);
            model.set(ActionProperties.BUTTON_STATE, currentTab != null ? DEFAULT : UNCLICKABLE);
            model.set(ActionProperties.ICON_TINT, null);
        }
    }

    private void showIncognitoSnackbar(View view) {
        mSnackbarManager.showSnackbar(
                Snackbar.make(
                                mActivity.getString(R.string.glic_incognito_not_available),
                                null,
                                Snackbar.TYPE_NOTIFICATION,
                                Snackbar.UMA_UNKNOWN)
                        .setDuration(SnackbarManager.DEFAULT_SNACKBAR_DURATION_MS));
    }

    private @Nullable PropertyModel getModelSupplierOrNull() {
        return mGlicActionModelSupplier != null ? mGlicActionModelSupplier.get() : null;
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
