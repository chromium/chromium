// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.history;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.app.Activity;

import org.chromium.base.supplier.OneshotSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.hub.LoadHint;
import org.chromium.chrome.browser.hub.Pane;
import org.chromium.chrome.browser.hub.PaneBase;
import org.chromium.chrome.browser.hub.PaneHubController;
import org.chromium.chrome.browser.hub.PaneId;
import org.chromium.chrome.browser.hub.ResourceButtonData;
import org.chromium.chrome.browser.profiles.ProfileProvider;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.ui.base.ActivityResultTracker;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modaldialog.ModalDialogManager;

import java.util.function.DoubleConsumer;
import java.util.function.Supplier;

/** A {@link Pane} representing history. */
@NullMarked
public class HistoryPane extends PaneBase {
    private final OneshotSupplier<ProfileProvider> mProfileProviderSupplier;
    private final WindowAndroid mWindowAndroid;
    private final Activity mActivity;
    private final SnackbarManager mSnackbarManager;
    private final ActivityResultTracker mActivityResultTracker;
    private final Supplier<@Nullable ModalDialogManager> mModalDialogManagerSupplier;
    private final Supplier<BottomSheetController> mBottomSheetController;
    private final Supplier<@Nullable Tab> mTabSupplier;

    private @Nullable HistoryManager mHistoryManager;
    private @Nullable PaneHubController mPaneHubController;

    /**
     * @param profileProviderSupplier Used as a dependency to HistoryManager.
     * @param onToolbarAlphaChange Observer to notify when alpha changes during animations.
     * @param windowAndroid Used as a dependency to HistoryManager.
     * @param activity Used as a dependency to HistoryManager.
     * @param snackbarManager Used as a dependency to HistoryManager.
     * @param bottomSheetController Used as a dependency to HistoryManager.
     * @param modalDialogManagerSupplier Used as a dependency to HistoryManager.
     * @param activityResultTracker Used as a dependency to HistoryManager.
     * @param tabSupplier Used as a dependency to HistoryManager.
     */
    public HistoryPane(
            OneshotSupplier<ProfileProvider> profileProviderSupplier,
            DoubleConsumer onToolbarAlphaChange,
            WindowAndroid windowAndroid,
            Activity activity,
            SnackbarManager snackbarManager,
            Supplier<BottomSheetController> bottomSheetController,
            Supplier<@Nullable ModalDialogManager> modalDialogManagerSupplier,
            ActivityResultTracker activityResultTracker,
            Supplier<@Nullable Tab> tabSupplier) {
        super(PaneId.HISTORY, activity, onToolbarAlphaChange);
        mReferenceButtonDataSupplier.set(
                new ResourceButtonData(
                        R.string.menu_history, R.string.menu_history, R.drawable.ic_history_24dp));

        mWindowAndroid = windowAndroid;
        mActivity = activity;
        mSnackbarManager = snackbarManager;
        mBottomSheetController = bottomSheetController;
        mModalDialogManagerSupplier = modalDialogManagerSupplier;
        mActivityResultTracker = activityResultTracker;
        mProfileProviderSupplier = profileProviderSupplier;
        mTabSupplier = tabSupplier;
    }

    @Override
    public void destroy() {
        destroyManagerAndRemoveView();
    }

    @Override
    public void setPaneHubController(@Nullable PaneHubController paneHubController) {
        mPaneHubController = paneHubController;
    }

    @Override
    public void notifyLoadHint(@LoadHint int loadHint) {
        if (loadHint == LoadHint.HOT && mHistoryManager == null) {
            mHistoryManager =
                    new HistoryManager(
                            assumeNonNull(mProfileProviderSupplier.get()).getOriginalProfile(),
                            mWindowAndroid,
                            mActivity,
                            /* isSeparateActivity= */ false,
                            mSnackbarManager,
                            mBottomSheetController,
                            mModalDialogManagerSupplier,
                            mActivityResultTracker,
                            mTabSupplier,
                            new BrowsingHistoryBridge(
                                    mProfileProviderSupplier.get().getOriginalProfile()),
                            new HistoryUmaRecorder(),
                            /* clientPackageName= */ null,
                            /* shouldShowClearData= */ true,
                            /* launchedForApp= */ false,
                            /* showAppFilter= */ true,
                            this::onHistoryItemOpened,
                            // TODO(crbug.com/427776544): make history pane support edge to edge.
                            /* edgeToEdgePadAdjusterGenerator= */ null);
            mRootView.addView(mHistoryManager.getView());
        } else if (loadHint == LoadHint.COLD) {
            destroyManagerAndRemoveView();
        }
    }

    private void onHistoryItemOpened() {
        assert mPaneHubController != null;
        mPaneHubController.selectTabAndHideHub(Tab.INVALID_TAB_ID);
    }

    private void destroyManagerAndRemoveView() {
        if (mHistoryManager != null) {
            mHistoryManager.onDestroyed();
            mHistoryManager = null;
        }
        mRootView.removeAllViews();
    }
}
