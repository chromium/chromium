// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant;

import android.app.Activity;
import android.content.Context;
import android.content.res.Configuration;
import android.view.Window;

import androidx.annotation.Nullable;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.base.task.PostTask;
import org.chromium.chrome.autofill_assistant.R;
import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.app.ChromeActivity;
import org.chromium.chrome.browser.autofill_assistant.carousel.AssistantChip;
import org.chromium.chrome.browser.autofill_assistant.metrics.DropOutReason;
import org.chromium.chrome.browser.autofill_assistant.overlay.AssistantOverlayCoordinator;
import org.chromium.chrome.browser.customtabs.CustomTabActivity;
import org.chromium.chrome.browser.feedback.ScreenshotMode;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabUtils;
import org.chromium.chrome.browser.ui.TabObscuringHandler;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager.SnackbarController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.SheetState;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetControllerProvider;
import org.chromium.content_public.browser.UiThreadTaskTraits;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyObservable;
import org.chromium.ui.modelutil.PropertyObservable.PropertyObserver;

import java.util.ArrayList;
import java.util.HashSet;
import java.util.List;
import java.util.Set;

/**
 * Bridge to native side autofill_assistant::UiControllerAndroid. It allows native side to control
 * Autofill Assistant related UIs and forward UI events to native side.
 * This controller is purely a translation and forwarding layer between Native side and the
 * different Java coordinators.
 */
@JNINamespace("autofill_assistant")
// TODO(crbug.com/806868): This class should be removed once all logic is in native side and the
// model is directly modified by the native AssistantMediator.
public class AutofillAssistantUiController {
    private static Set<ChromeActivity> sActiveChromeActivities;
    private long mNativeUiController;

    private final ChromeActivity mActivity;
    private final AssistantCoordinator mCoordinator;
    private final ActivityTabProvider.ActivityTabTabObserver mActivityTabObserver;
    private WebContents mWebContents;
    private SnackbarController mSnackbarController;

    /**
     * Getter for the current profile while assistant is running. Since autofill assistant is only
     * available in regular mode and there is only one regular profile in android, this method
     * returns {@link Profile#getLastUsedRegularProfile()}.
     *
     * TODO(b/161519639): Return current profile to support multi profiles, instead of returning
     * always regular profile. This could be achieve by retrieving profile from native and using it
     * where the profile is needed on Java side.
     * @return The current regular profile.
     */
    public static Profile getProfile() {
        return Profile.getLastUsedRegularProfile();
    }
    /**
     * Finds an activity to which a AA UI can be added.
     *
     * <p>The activity must not already have an AA UI installed.
     */
    @CalledByNative
    @Nullable
    private static ChromeActivity findAppropriateActivity(WebContents webContents) {
        ChromeActivity activity = ChromeActivity.fromWebContents(webContents);
        if (activity != null && isActive(activity)) {
            return null;
        }

        return activity;
    }

    /**
     * Returns {@code true} if an AA UI is active on the given activity.
     *
     * <p>Used to avoid creating duplicate coordinators views.
     *
     * <p>TODO(crbug.com/806868): Refactor to have AssistantCoordinator owned by the activity, so
     * it's easy to guarantee that there will be at most one per activity.
     */
    private static boolean isActive(ChromeActivity activity) {
        if (sActiveChromeActivities == null) {
            return false;
        }

        return sActiveChromeActivities.contains(activity);
    }

    @CalledByNative
    private static AutofillAssistantUiController create(ChromeActivity activity,
            boolean allowTabSwitching, long nativeUiController,
            @Nullable AssistantOverlayCoordinator overlayCoordinator) {
        BottomSheetController sheetController =
                BottomSheetControllerProvider.from(activity.getWindowAndroid());
        assert activity != null;
        assert sheetController != null;

        if (sActiveChromeActivities == null) {
            sActiveChromeActivities = new HashSet<>();
        }
        sActiveChromeActivities.add(activity);

        // TODO(crbug.com/1048983): Have the params be passed in to the constructor directly rather
        //         than obtaining them from ChromeActivity getters.
        return new AutofillAssistantUiController(activity, sheetController,
                activity.getTabObscuringHandler(), allowTabSwitching, nativeUiController,
                overlayCoordinator);
    }

    private AutofillAssistantUiController(ChromeActivity activity, BottomSheetController controller,
            TabObscuringHandler tabObscuringHandler, boolean allowTabSwitching,
            long nativeUiController, @Nullable AssistantOverlayCoordinator overlayCoordinator) {
        mNativeUiController = nativeUiController;
        mActivity = activity;
        mCoordinator = new AssistantCoordinator(activity, controller, tabObscuringHandler,
                overlayCoordinator, this::safeNativeOnKeyboardVisibilityChanged);
        mActivityTabObserver =
                new ActivityTabProvider.ActivityTabTabObserver(
                        activity.getActivityTabProvider(), /* shouldTrigger = */ true) {
                    @Override
                    protected void onObservingDifferentTab(Tab tab, boolean hint) {
                        if (mWebContents == null) {
                            if (!hint) {
                                // This particular scenario would happen only if we're switching
                                // from a tab with no Autofill Assistant running to a tab with AA
                                // running with no tab switching hinting (i.e. a first notification
                                // with |hint| set to true).
                                // In this case the native side is not yet fully initialized, so we
                                // need to wait for the web contents to be set from native before
                                // notifying native that the tab was selected.
                                setWebContentObserver(tab);
                            }
                            return;
                        }

                        if (!allowTabSwitching) {
                            if (tab == null || tab.getWebContents() != mWebContents) {
                                safeNativeOnFatalError(
                                        activity.getString(R.string.autofill_assistant_give_up),
                                        DropOutReason.TAB_CHANGED);
                            }
                            return;
                        }

                        // Get rid of any undo snackbars right away before switching tabs, to avoid
                        // confusion.
                        dismissSnackbar();

                        if (tab == null) {
                            safeOnTabSwitched(getModel().getBottomSheetState(),
                                    /* activityChanged = */ false);
                            // A null tab indicates that there's no selected tab; Most likely, we're
                            // in the process of selecting a new tab. Hide the UI for possible reuse
                            // later.
                            safeNativeSetVisible(false);
                        } else if (tab.getWebContents() == mWebContents) {
                            // The original tab was re-selected. Show it again and force an
                            // expansion on the bottom sheet.
                            if (!hint) {
                                // Here and below, we're only interested in restoring the UI for the
                                // case where hint is false, meaning that the tab is shown. This is
                                // the only way to be sure that the bottomsheet is unsuppressed when
                                // we try to restore the status to what it was prior to switching.
                                safeOnTabSelected();
                            }
                        } else {
                            //
                            safeOnTabSwitched(getModel().getBottomSheetState(),
                                    /* activityChanged = */ false);
                            // A new tab was selected. If Autofill Assistant is running on it,
                            // attach the UI to that other instance, otherwise destroy the UI.
                            AutofillAssistantClient.fromWebContents(mWebContents)
                                    .transferUiTo(tab.getWebContents());
                            if (!hint) {
                                safeOnTabSelected();
                            }
                        }
                    }

                    @Override
                    public void onActivityAttachmentChanged(
                            Tab tab, @Nullable WindowAndroid window) {
                        if (mWebContents == null) return;

                        if (window == null && tab.getWebContents() == mWebContents) {
                            if (!allowTabSwitching) {
                                safeNativeStop(DropOutReason.TAB_DETACHED);
                                return;
                            }

                            safeOnTabSwitched(
                                    getModel().getBottomSheetState(), /* activityChanged = */ true);
                            // If we have an open snackbar, execute the callback immediately. This
                            // may shut down the Autofill Assistant.
                            if (mSnackbarController != null) {
                                mSnackbarController.onDismissNoAction(/* actionData= */ null);
                            }
                            AutofillAssistantClient.fromWebContents(mWebContents).destroyUi();
                        }
                    }
                };
    }

    private void setWebContentObserver(Tab tab) {
        getModel().addObserver(new PropertyObserver<PropertyKey>() {
            @Override
            public void onPropertyChanged(
                    PropertyObservable<PropertyKey> source, @Nullable PropertyKey propertyKey) {
                if (AssistantModel.WEB_CONTENTS == propertyKey) {
                    getModel().removeObserver(this);
                    if (tab != null
                            && tab.getWebContents()
                                    == getModel().get(AssistantModel.WEB_CONTENTS)) {
                        safeOnTabSelected();
                    }
                }
            }
        });
    }

    // Native => Java methods.

    // TODO(crbug.com/806868): Some of these functions still have a little bit of logic (e.g. make
    // the progress bar pulse when hiding overlay). Maybe it would be better to forward all calls to
    // AssistantCoordinator (that way this bridge would only have a reference to that one) which in
    // turn will forward calls to the other sub coordinators. The main reason this is not done yet
    // is to avoid boilerplate.

    @CalledByNative
    private void setWebContents(@Nullable WebContents webContents) {
        mWebContents = webContents;
    }

    @CalledByNative
    private AssistantModel getModel() {
        return mCoordinator.getModel();
    }

    @CalledByNative
    private void clearNativePtr() {
        mNativeUiController = 0;
        mActivityTabObserver.destroy();
        mCoordinator.destroy();
        sActiveChromeActivities.remove(mActivity);
    }

    /**
     * Close CCT after the current task has finished running - usually after Autofill Assistant has
     * finished shutting itself down.
     */
    @CalledByNative
    private void scheduleCloseCustomTab() {
        if (mActivity instanceof CustomTabActivity) {
            PostTask.postTask(UiThreadTaskTraits.DEFAULT, mActivity::finish);
        }
    }

    @CalledByNative
    private void showContentAndExpandBottomSheet() {
        mCoordinator.getBottomBarCoordinator().showContent(
                /* shouldExpand = */ true, /* animate = */ true);
    }

    @CalledByNative
    private void expandBottomSheet() {
        mCoordinator.getBottomBarCoordinator().expand();
    }

    @CalledByNative
    private void collapseBottomSheet() {
        mCoordinator.getBottomBarCoordinator().collapse();
    }

    @CalledByNative
    private void showFeedback(String debugContext, @ScreenshotMode int screenshotMode) {
        mCoordinator.showFeedback(debugContext, screenshotMode);
    }

    @CalledByNative
    private boolean isKeyboardShown() {
        return mCoordinator.getKeyboardCoordinator().isKeyboardShown();
    }

    @CalledByNative
    private void hideKeyboard() {
        mCoordinator.getKeyboardCoordinator().hideKeyboard();
    }

    @CalledByNative
    private void restoreBottomSheetState(@SheetState int state) {
        mCoordinator.getBottomBarCoordinator().restoreState(state);
    }

    @CalledByNative
    private void hideKeyboardIfFocusNotOnText() {
        mCoordinator.getKeyboardCoordinator().hideKeyboardIfFocusNotOnText();
    }

    @CalledByNative
    private void showSnackbar(int delayMs, String message) {
        mSnackbarController = AssistantSnackbar.show(mActivity, mActivity.getSnackbarManager(),
                delayMs, message, this::safeSnackbarResult);
    }

    private void dismissSnackbar() {
        if (mSnackbarController != null) {
            mActivity.getSnackbarManager().dismissSnackbars(mSnackbarController);
            mSnackbarController = null;
        }
    }

    /** Creates an empty list of chips. */
    @CalledByNative
    private static List<AssistantChip> createChipList() {
        return new ArrayList<>();
    }

    /**
     * Creates an action button which executes the action {@code actionIndex}.
     */
    @CalledByNative
    private AssistantChip createActionButton(int icon, String text, int actionIndex,
            boolean disabled, boolean sticky, boolean visible,
            @Nullable String contentDescription) {
        AssistantChip chip = AssistantChip.createHairlineAssistantChip(
                icon, text, disabled, sticky, visible, contentDescription);
        chip.setSelectedListener(() -> safeNativeOnUserActionSelected(actionIndex));
        return chip;
    }

    /**
     * Creates a highlighted action button which executes the action {@code actionIndex}.
     */
    @CalledByNative
    private AssistantChip createHighlightedActionButton(int icon, String text, int actionIndex,
            boolean disabled, boolean sticky, boolean visible,
            @Nullable String contentDescription) {
        AssistantChip chip = AssistantChip.createHighlightedAssistantChip(
                icon, text, disabled, sticky, visible, contentDescription);
        chip.setSelectedListener(() -> safeNativeOnUserActionSelected(actionIndex));
        return chip;
    }

    /**
     * Creates a cancel action button. If the keyboard is currently shown, it dismisses the
     * keyboard. Otherwise, it shows the snackbar and then executes {@code actionIndex}, or shuts
     * down Autofill Assistant if {@code actionIndex} is {@code -1}.
     */
    @CalledByNative
    private AssistantChip createCancelButton(int icon, String text, int actionIndex,
            boolean disabled, boolean sticky, boolean visible,
            @Nullable String contentDescription) {
        AssistantChip chip = AssistantChip.createHairlineAssistantChip(
                icon, text, disabled, sticky, visible, contentDescription);
        chip.setSelectedListener(() -> safeNativeOnCancelButtonClicked(actionIndex));
        return chip;
    }

    /**
     * Adds a close action button to the chip list, which shuts down Autofill Assistant.
     */
    @CalledByNative
    private AssistantChip createCloseButton(int icon, String text, boolean disabled, boolean sticky,
            boolean visible, @Nullable String contentDescription) {
        AssistantChip chip = AssistantChip.createHairlineAssistantChip(
                icon, text, disabled, sticky, visible, contentDescription);
        chip.setSelectedListener(() -> safeNativeOnCloseButtonClicked());
        return chip;
    }

    /**
     * Creates a feedback button button. It shows the feedback form and then *directly* executes
     * {@code actionIndex}.
     */
    @CalledByNative
    private AssistantChip createFeedbackButton(int icon, String text, int actionIndex,
            boolean disabled, boolean sticky, boolean visible,
            @Nullable String contentDescription) {
        AssistantChip chip = AssistantChip.createHairlineAssistantChip(
                icon, text, disabled, sticky, visible, contentDescription);
        chip.setSelectedListener(() -> safeNativeOnFeedbackButtonClicked(actionIndex));
        return chip;
    }

    // TODO(arbesser): Remove this and use methods in {@code AssistantChip} instead.
    @CalledByNative
    private static void appendChipToList(List<AssistantChip> chips, AssistantChip chip) {
        chips.add(chip);
    }

    @CalledByNative
    private void setActions(List<AssistantChip> chips) {
        // TODO(b/144075373): Move this to AssistantCarouselModel.
        getModel().getActionsModel().setChips(chips);
    }

    @CalledByNative
    private void setDisableChipChangeAnimations(boolean disable) {
        // TODO(b/144075373): Move this to AssistantCarouselModel.
        getModel().getActionsModel().setDisableChangeAnimations(disable);
    }

    @CalledByNative
    private void setViewportMode(@AssistantViewportMode int mode) {
        mCoordinator.getBottomBarCoordinator().setViewportMode(mode);
    }

    @CalledByNative
    private void setPeekMode(@AssistantPeekHeightCoordinator.PeekMode int peekMode) {
        mCoordinator.getBottomBarCoordinator().setPeekMode(peekMode);
    }

    @CalledByNative
    private Context getContext() {
        return mActivity;
    }

    @CalledByNative
    private int[] getWindowSize() {
        Activity activity = TabUtils.getActivity(TabUtils.fromWebContents(mWebContents));
        if (activity == null) {
            return null;
        }
        Window window = activity.getWindow();
        if (window == null) {
            return null;
        }
        return new int[] {window.getDecorView().getWidth(), window.getDecorView().getHeight()};
    }

    @CalledByNative
    private int getScreenOrientation() {
        Activity activity = TabUtils.getActivity(TabUtils.fromWebContents(mWebContents));
        if (activity == null) {
            return Configuration.ORIENTATION_UNDEFINED;
        }
        return activity.getResources().getConfiguration().orientation;
    }

    // Native methods.
    private void safeSnackbarResult(boolean undo) {
        if (mSnackbarController != null && mNativeUiController != 0) {
            AutofillAssistantUiControllerJni.get().snackbarResult(
                    mNativeUiController, AutofillAssistantUiController.this, undo);
            mSnackbarController = null;
        }
    }

    private void safeNativeStop(@DropOutReason int reason) {
        if (mNativeUiController != 0) {
            AutofillAssistantUiControllerJni.get().stop(
                    mNativeUiController, AutofillAssistantUiController.this, reason);
        }
    }

    private void safeNativeOnFatalError(String message, @DropOutReason int reason) {
        if (mNativeUiController != 0) {
            AutofillAssistantUiControllerJni.get().onFatalError(
                    mNativeUiController, AutofillAssistantUiController.this, message, reason);
        }
    }

    private void safeNativeOnUserActionSelected(int index) {
        if (mNativeUiController != 0) {
            AutofillAssistantUiControllerJni.get().onUserActionSelected(
                    mNativeUiController, AutofillAssistantUiController.this, index);
        }
    }

    private void safeNativeOnCancelButtonClicked(int index) {
        if (mNativeUiController != 0) {
            AutofillAssistantUiControllerJni.get().onCancelButtonClicked(
                    mNativeUiController, AutofillAssistantUiController.this, index);
        }
    }

    private void safeNativeOnCloseButtonClicked() {
        if (mNativeUiController != 0) {
            AutofillAssistantUiControllerJni.get().onCloseButtonClicked(
                    mNativeUiController, AutofillAssistantUiController.this);
        }
    }

    private void safeNativeOnFeedbackButtonClicked(int index) {
        if (mNativeUiController != 0) {
            AutofillAssistantUiControllerJni.get().onFeedbackButtonClicked(
                    mNativeUiController, AutofillAssistantUiController.this, index);
        }
    }

    private void safeNativeOnKeyboardVisibilityChanged(boolean visible) {
        if (mNativeUiController != 0) {
            AutofillAssistantUiControllerJni.get().onKeyboardVisibilityChanged(
                    mNativeUiController, AutofillAssistantUiController.this, visible);
        }
    }

    private void safeNativeSetVisible(boolean visible) {
        if (mNativeUiController != 0) {
            AutofillAssistantUiControllerJni.get().setVisible(
                    mNativeUiController, AutofillAssistantUiController.this, visible);
        }
    }

    private void safeOnTabSwitched(@SheetState int state, boolean activityChanged) {
        if (mNativeUiController != 0) {
            AutofillAssistantUiControllerJni.get().onTabSwitched(mNativeUiController,
                    AutofillAssistantUiController.this, state, activityChanged);
        }
    }

    private void safeOnTabSelected() {
        if (mNativeUiController != 0) {
            AutofillAssistantUiControllerJni.get().onTabSelected(
                    mNativeUiController, AutofillAssistantUiController.this);
        }
    }

    @NativeMethods
    interface Natives {
        void snackbarResult(
                long nativeUiControllerAndroid, AutofillAssistantUiController caller, boolean undo);
        void stop(long nativeUiControllerAndroid, AutofillAssistantUiController caller,
                @DropOutReason int reason);
        void onFatalError(long nativeUiControllerAndroid, AutofillAssistantUiController caller,
                String message, @DropOutReason int reason);
        void onUserActionSelected(
                long nativeUiControllerAndroid, AutofillAssistantUiController caller, int index);
        void onCancelButtonClicked(
                long nativeUiControllerAndroid, AutofillAssistantUiController caller, int index);
        void onCloseButtonClicked(
                long nativeUiControllerAndroid, AutofillAssistantUiController caller);
        void onFeedbackButtonClicked(
                long nativeUiControllerAndroid, AutofillAssistantUiController caller, int index);
        void onKeyboardVisibilityChanged(long nativeUiControllerAndroid,
                AutofillAssistantUiController caller, boolean visible);
        void setVisible(long nativeUiControllerAndroid, AutofillAssistantUiController caller,
                boolean visible);
        void onTabSwitched(long nativeUiControllerAndroid, AutofillAssistantUiController caller,
                @SheetState int state, boolean activityChanged);
        void onTabSelected(long nativeUiControllerAndroid, AutofillAssistantUiController caller);
    }
}
