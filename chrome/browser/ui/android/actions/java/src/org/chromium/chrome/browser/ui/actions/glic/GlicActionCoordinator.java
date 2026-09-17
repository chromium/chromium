// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.actions.glic;

import static org.chromium.chrome.browser.ui.actions.button.ButtonState.DEFAULT;
import static org.chromium.chrome.browser.ui.actions.button.ButtonState.UNCLICKABLE;

import android.app.Activity;
import android.graphics.drawable.Drawable;
import android.view.View;

import androidx.annotation.ColorInt;
import androidx.annotation.Px;
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
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabObserver;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.ui.actions.ActionId;
import org.chromium.chrome.browser.ui.actions.ActionProperties;
import org.chromium.chrome.browser.ui.actions.ActionRegistry;
import org.chromium.chrome.browser.ui.actions.R;
import org.chromium.chrome.browser.ui.bottombar.BottomBarConfigUtils;
import org.chromium.chrome.browser.ui.bottombar.BottomBarMetrics;
import org.chromium.chrome.browser.ui.browser_window.ChromeAndroidTask;
import org.chromium.chrome.browser.ui.messages.snackbar.Snackbar;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.browser.user_education.UserEducationHelper;
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
    private final UserEducationHelper mUserEducationHelper;
    private @Nullable GlicTaskMenuCoordinator mTaskMenuCoordinator;
    private final Drawable mDefaultDrawable;
    private @Nullable Drawable mFilledDrawable;
    private @Nullable Drawable mWorkingDrawable;
    private @Nullable Drawable mReviewDrawable;

    public GlicActionCoordinator(
            Activity activity,
            ActionRegistry actionRegistry,
            GlicButtonDelegate toggleGlicCallback,
            NullableObservableSupplier<Tab> tabSupplier,
            Supplier<@Nullable ChromeAndroidTask> taskSupplier,
            BrowserControlsVisibilityManager browserControlsVisibilityManager,
            Supplier<@Nullable TabModelSelector> tabModelSelectorSupplier,
            SnackbarManager snackbarManager,
            UserEducationHelper userEducationHelper) {
        mActivity = activity;
        mToggleGlicCallback = toggleGlicCallback;
        mTabSupplier = tabSupplier;
        mTabModelSelectorSupplier = tabModelSelectorSupplier;
        mSnackbarManager = snackbarManager;
        mUserEducationHelper = userEducationHelper;
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
                        new TabObserver() {
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
    }

    private Drawable getFilledDrawable() {
        if (mFilledDrawable == null) {
            mFilledDrawable =
                    AppCompatResources.getDrawable(mActivity, R.drawable.ic_spark_filled_24dp);
        }
        return mFilledDrawable;
    }

    private Drawable getWorkingDrawable() {
        if (mWorkingDrawable == null) {
            // Create a separate instance of the spark icon for mWorkingDrawable.
            // Sharing the same drawable instance (mFilledDrawable) across multiple states
            // causes the ImageView's callback clearing logic to break the callback chain of
            // the child drawable. This results in the spark icon layer losing its themed tint
            // initially when entering the WORKING state, until  a hover/state change forces a full
            // view redraw.
            Drawable workingSparkIcon =
                    AppCompatResources.getDrawable(mActivity, R.drawable.ic_spark_filled_24dp);
            mWorkingDrawable = GlicUiHelper.createWorkingDrawable(mActivity, workingSparkIcon);
        }
        return mWorkingDrawable;
    }

    /** Returns the dirty-dot drawable shared by the NEEDS_REVIEW and DONE states. */
    private Drawable getReviewDrawable() {
        if (mReviewDrawable == null) {
            Drawable sparkIcon =
                    AppCompatResources.getDrawable(mActivity, R.drawable.ic_spark_filled_24dp);
            @ColorInt
            int dotColor = mActivity.getColor(R.color.default_icon_color_accent1_baseline);
            @Px
            int dotSize =
                    mActivity.getResources().getDimensionPixelSize(R.dimen.glic_dirty_dot_size);
            mReviewDrawable = new DirtyDotDrawableWrapper(sparkIcon, dotColor, dotSize);
        }
        return mReviewDrawable;
    }

    private void onModelChanged(@Nullable PropertyModel model) {
        if (model == null) return;
        model.set(ActionProperties.ON_PRESS_CALLBACK, this::onGlicActionPressed);
        model.set(ActionProperties.USER_EDUCATION_HELPER, mUserEducationHelper);
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
                    desiredDrawable = getWorkingDrawable();
                    break;
                case GlicButtonStateController.ButtonState.NEEDS_REVIEW:
                case GlicButtonStateController.ButtonState.DONE:
                    // These two states intentionally share one drawable instance, as they did
                    // when both were built eagerly in the constructor.
                    desiredDrawable = getReviewDrawable();
                    break;
                case GlicButtonStateController.ButtonState.DEFAULT:
                default:
                    desiredDrawable = isPanelOpen ? getFilledDrawable() : mDefaultDrawable;
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

        BottomBarMetrics.recordGlicButtonState(
                mStateController.getButtonState(), mStateController.isPanelOpen());

        List<ActorTask> tasks = mStateController.getActiveTasks();

        boolean isOnActingTab =
                currentTab != null
                        && mStateController.getActiveTaskIdOnTab(currentTab.getId()) != null;

        // If there are no tasks, or we are already on the tab with the active task, just toggle
        // Glic.
        if (tasks == null || tasks.isEmpty() || isOnActingTab) {
            BottomBarMetrics.recordGlicConvoResult(mStateController.isPanelOpen());
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
                            GlicInvocationSource.TOOLBAR_BUTTON,
                            GlicTaskMenuCoordinator.ButtonSource.BOTTOM_BAR);
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
