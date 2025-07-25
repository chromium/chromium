// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import android.app.Activity;
import android.view.View;

import androidx.annotation.NonNull;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.compositor.CompositorViewHolder;
import org.chromium.chrome.browser.keyboard_accessory.ManualFillingComponent;
import org.chromium.chrome.browser.ui.browser_window.ChromeAndroidTask;
import org.chromium.chrome.browser.ui.browser_window.ChromeAndroidTaskTrackerFactory;
import org.chromium.ui.base.ActivityKeyboardVisibilityDelegate;
import org.chromium.ui.base.ActivityWindowAndroid;
import org.chromium.ui.base.IntentRequestTracker;
import org.chromium.ui.insets.InsetObserver;
import org.chromium.ui.modaldialog.ModalDialogManager;

import java.lang.ref.WeakReference;

/**
 * The window that has access to the main activity and is able to create and receive intents,
 * and show error messages.
 */
public class ChromeWindow extends ActivityWindowAndroid {
    /** Interface allowing to inject a different keyboard delegate for testing. */
    @VisibleForTesting
    public interface KeyboardVisibilityDelegateFactory {
        ChromeKeyboardVisibilityDelegate create(
                @NonNull WeakReference<Activity> activity,
                @NonNull Supplier<ManualFillingComponent> manualFillingComponentSupplier);
    }

    private static KeyboardVisibilityDelegateFactory sKeyboardVisibilityDelegateFactory =
            ChromeKeyboardVisibilityDelegate::new;

    private final Supplier<CompositorViewHolder> mCompositorViewHolderSupplier;
    private final Supplier<ModalDialogManager> mModalDialogManagerSupplier;

    /**
     * Creates Chrome specific ActivityWindowAndroid.
     *
     * @param activity The activity that owns the ChromeWindow.
     * @param compositorViewHolderSupplier Supplies the {@link CompositorViewHolder}.
     * @param modalDialogManagerSupplier Supplies the {@link ModalDialogManager}.
     * @param manualFillingComponentSupplier Supplies the {@link ManualFillingComponent}.
     * @param intentRequestTracker The {@link IntentRequestTracker} of the current activity.
     */
    public ChromeWindow(
            @NonNull Activity activity,
            @NonNull Supplier<CompositorViewHolder> compositorViewHolderSupplier,
            @NonNull Supplier<ModalDialogManager> modalDialogManagerSupplier,
            @NonNull Supplier<ManualFillingComponent> manualFillingComponentSupplier,
            @NonNull IntentRequestTracker intentRequestTracker,
            @NonNull InsetObserver insetObserver) {
        this(
                activity,
                compositorViewHolderSupplier,
                modalDialogManagerSupplier,
                sKeyboardVisibilityDelegateFactory.create(
                        new WeakReference<>(activity), manualFillingComponentSupplier),
                /* activityTopResumedSupported= */ true,
                intentRequestTracker,
                insetObserver);
    }

    /**
     * Creates Chrome specific ActivityWindowAndroid.
     *
     * @param activity The activity that owns the ChromeWindow.
     * @param compositorViewHolderSupplier Supplies the {@link CompositorViewHolder}.
     * @param modalDialogManagerSupplier Supplies the {@link ModalDialogManager}.
     * @param activityKeyboardVisibilityDelegate Delegate to handle keyboard visibility.
     * @param intentRequestTracker The {@link IntentRequestTracker} of the current activity.
     */
    public ChromeWindow(
            @NonNull Activity activity,
            @NonNull Supplier<CompositorViewHolder> compositorViewHolderSupplier,
            @NonNull Supplier<ModalDialogManager> modalDialogManagerSupplier,
            @NonNull ActivityKeyboardVisibilityDelegate activityKeyboardVisibilityDelegate,
            boolean activityTopResumedSupported,
            IntentRequestTracker intentRequestTracker,
            @NonNull InsetObserver insetObserver) {
        super(
                activity,
                /* listenToActivityState= */ true,
                activityKeyboardVisibilityDelegate,
                activityTopResumedSupported,
                intentRequestTracker,
                insetObserver,
                /* trackOcclusion= */ true);
        assert insetObserver != null;
        mCompositorViewHolderSupplier = compositorViewHolderSupplier;
        mModalDialogManagerSupplier = modalDialogManagerSupplier;
    }

    @Override
    public void destroy() {
        clearActivityWindowAndroidFromChromeAndroidTask();

        // It's not 100% correct to destroy the ChromeAndroidTask here as a ChromeAndroidTask
        // is meant to track an Android Task, but a ChromeWindow is associated with a
        // ChromeActivity.
        //
        // However, as of July 22, 2025, Android framework doesn't provide an API that listens for
        // Task removal, so we need to destroy the ChromeAndroidTask here as a workaround.
        //
        // In the future, we can register a Task listener when a ChromeAndroidTask is created, then
        // destroy it when notified of the Task removal.
        destroyChromeAndroidTask();

        super.destroy();
    }

    @Override
    public View getReadbackView() {
        return mCompositorViewHolderSupplier.get() == null
                ? null
                : mCompositorViewHolderSupplier.get().getActiveSurfaceView();
    }

    @Override
    public ModalDialogManager getModalDialogManager() {
        // TODO(crbug.com/40160045): Move ModalDialogManager to UnownedUserData.
        return mModalDialogManagerSupplier.get();
    }

    @VisibleForTesting
    public static void setKeyboardVisibilityDelegateFactory(
            KeyboardVisibilityDelegateFactory factory) {
        sKeyboardVisibilityDelegateFactory = factory;
    }

    @VisibleForTesting
    public static void resetKeyboardVisibilityDelegateFactory() {
        setKeyboardVisibilityDelegateFactory(ChromeKeyboardVisibilityDelegate::new);
    }

    /** See {@link ChromeAndroidTask#clearActivityWindowAndroid()}. */
    private void clearActivityWindowAndroidFromChromeAndroidTask() {
        var chromeAndroidTaskTracker = ChromeAndroidTaskTrackerFactory.getInstance();
        if (chromeAndroidTaskTracker == null) {
            return;
        }

        var chromeAndroidTask = chromeAndroidTaskTracker.get(getTaskId());
        if (chromeAndroidTask != null) {
            chromeAndroidTask.clearActivityWindowAndroid();
        }
    }

    private void destroyChromeAndroidTask() {
        var chromeAndroidTaskTracker = ChromeAndroidTaskTrackerFactory.getInstance();
        if (chromeAndroidTaskTracker != null) {
            chromeAndroidTaskTracker.remove(getTaskId());
        }
    }

    private int getTaskId() {
        Activity activity = getActivity().get();
        assert activity != null;

        return activity.getTaskId();
    }
}
