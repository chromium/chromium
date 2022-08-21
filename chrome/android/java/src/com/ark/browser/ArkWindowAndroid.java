package com.ark.browser;

import android.content.Context;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.chrome.browser.tab.TabDelegateFactory;
import org.chromium.components.browser_ui.modaldialog.AppModalPresenter;
import org.chromium.ui.base.ActivityKeyboardVisibilityDelegate;
import org.chromium.ui.base.ActivityWindowAndroid;
import org.chromium.ui.base.IntentRequestTracker;
import org.chromium.ui.modaldialog.ModalDialogManager;

public abstract class ArkWindowAndroid extends ActivityWindowAndroid {

    private ModalDialogManager mModalDialogManager;

    public ArkWindowAndroid(Context context, boolean listenToActivityState, IntentRequestTracker intentRequestTracker) {
        super(context, listenToActivityState, intentRequestTracker);
    }

    public ArkWindowAndroid(Context context, boolean listenToActivityState, @NonNull ActivityKeyboardVisibilityDelegate keyboardVisibilityDelegate, IntentRequestTracker intentRequestTracker) {
        super(context, listenToActivityState, keyboardVisibilityDelegate, intentRequestTracker);


    }

    public abstract TabDelegateFactory getTabDelegateFactory();

    public abstract ArkCompositorViewHolder getCompositorViewHolder();

    public @Nullable
    ModalDialogManager getModalDialogManager() {
        if (mModalDialogManager == null) {
            mModalDialogManager = new ModalDialogManager(
                    new AppModalPresenter(getActivity().get()), ModalDialogManager.ModalDialogType.APP);
        }
        return mModalDialogManager;
    }

}
