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
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.ui.base.ActivityKeyboardVisibilityDelegate;
import org.chromium.ui.base.ActivityWindowAndroid;
import org.chromium.ui.base.IntentRequestTracker;
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

    private final ActivityTabProvider mActivityTabProvider;
    private final Supplier<CompositorViewHolder> mCompositorViewHolderSupplier;
    private final Supplier<ModalDialogManager> mModalDialogManagerSupplier;

    /**
     * Creates Chrome specific ActivityWindowAndroid.
     * @param activity The activity that owns the ChromeWindow.
     * @param activityTabProvider Provides the current activity's {@link Tab}.
     * @param compositorViewHolderSupplier Supplies the {@link CompositorViewHolder}.
     * @param modalDialogManagerSupplier Supplies the {@link ModalDialogManager}.
     * @param manualFillingComponentSupplier Supplies the {@link ManualFillingComponent}.
     * @param intentRequestTracker The {@link IntentRequestTracker} of the current activity.
     */
    public ChromeWindow(
            @NonNull Activity activity,
            @NonNull ActivityTabProvider activityTabProvider,
            @NonNull Supplier<CompositorViewHolder> compositorViewHolderSupplier,
            @NonNull Supplier<ModalDialogManager> modalDialogManagerSupplier,
            @NonNull Supplier<ManualFillingComponent> manualFillingComponentSupplier,
            @NonNull IntentRequestTracker intentRequestTracker) {
        this(
                activity,
                activityTabProvider,
                compositorViewHolderSupplier,
                modalDialogManagerSupplier,
                sKeyboardVisibilityDelegateFactory.create(
                        new WeakReference<Activity>(activity), manualFillingComponentSupplier),
                intentRequestTracker);
    }

    /**
     * Creates Chrome specific ActivityWindowAndroid.
     * @param activity The activity that owns the ChromeWindow.
     * @param activityTabProvider Provides the current activity's {@link Tab}.
     * @param compositorViewHolderSupplier Supplies the {@link CompositorViewHolder}.
     * @param modalDialogManagerSupplier Supplies the {@link ModalDialogManager}.
     * @param activityKeyboardVisibilityDelegate Delegate to handle keyboard visibility.
     * @param intentRequestTracker The {@link IntentRequestTracker} of the current activity.
     */
    public ChromeWindow(
            @NonNull Activity activity,
            @NonNull ActivityTabProvider activityTabProvider,
            @NonNull Supplier<CompositorViewHolder> compositorViewHolderSupplier,
            @NonNull Supplier<ModalDialogManager> modalDialogManagerSupplier,
            @NonNull ActivityKeyboardVisibilityDelegate activityKeyboardVisibilityDelegate,
            IntentRequestTracker intentRequestTracker) {
        super(
                activity,
                /* listenToActivityState= */ true,
                activityKeyboardVisibilityDelegate,
                intentRequestTracker);
        mActivityTabProvider = activityTabProvider;
        mCompositorViewHolderSupplier = compositorViewHolderSupplier;
        mModalDialogManagerSupplier = modalDialogManagerSupplier;
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
}
