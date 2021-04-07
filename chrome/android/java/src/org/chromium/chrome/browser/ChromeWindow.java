// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import android.app.Activity;
import android.view.View;

import androidx.annotation.NonNull;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.compositor.CompositorViewHolder;
import org.chromium.chrome.browser.infobar.InfoBarIdentifier;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.ui.messages.infobar.SimpleConfirmInfoBarBuilder;
import org.chromium.ui.base.ActivityWindowAndroid;
import org.chromium.ui.modaldialog.ModalDialogManager;

import java.lang.ref.WeakReference;

/**
 * The window that has access to the main activity and is able to create and receive intents,
 * and show error messages.
 */
public class ChromeWindow extends ActivityWindowAndroid {
    /**
     * Interface allowing to inject a different keyboard delegate for testing.
     */
    @VisibleForTesting
    public interface KeyboardVisibilityDelegateFactory {
        ChromeKeyboardVisibilityDelegate create(WeakReference<Activity> activity);
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
     */
    public ChromeWindow(@NonNull Activity activity,
            @NonNull ActivityTabProvider activityTabProvider,
            @NonNull Supplier<CompositorViewHolder> compositorViewHolderSupplier,
            @NonNull Supplier<ModalDialogManager> modalDialogManagerSupplier) {
        super(activity);
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
        // TODO(crbug.com/1155658): Move ModalDialogManager to UnownedUserData.
        return mModalDialogManagerSupplier.get();
    }

    @Override
    protected ChromeKeyboardVisibilityDelegate createKeyboardVisibilityDelegate() {
        return sKeyboardVisibilityDelegateFactory.create(getActivity());
    }

    /**
     * Shows an infobar error message overriding the WindowAndroid implementation.
     */
    @Override
    protected void showCallbackNonExistentError(String error) {
        Tab tab = mActivityTabProvider.get();

        if (tab != null) {
            SimpleConfirmInfoBarBuilder.create(tab.getWebContents(),
                    InfoBarIdentifier.WINDOW_ERROR_INFOBAR_DELEGATE_ANDROID, error, false);
        } else {
            super.showCallbackNonExistentError(error);
        }
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
