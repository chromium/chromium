// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.history;

import static org.chromium.build.NullUtil.assertNonNull;
import static org.chromium.build.NullUtil.assumeNonNull;

import android.content.Intent;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;

import org.chromium.base.CallbackUtils;
import org.chromium.base.IntentUtils;
import org.chromium.base.supplier.SupplierUtils;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.SnackbarActivity;
import org.chromium.chrome.browser.back_press.BackPressHelper;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.ui.edge_to_edge.EdgeToEdgeControllerFactory;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetControllerFactory;
import org.chromium.components.browser_ui.bottomsheet.ManagedBottomSheetController;
import org.chromium.components.browser_ui.modaldialog.AppModalPresenter;
import org.chromium.components.browser_ui.widget.scrim.ScrimManager;
import org.chromium.components.browser_ui.widget.scrim.ScrimManager.ScrimClient;
import org.chromium.ui.base.ActivityWindowAndroid;
import org.chromium.ui.base.IntentRequestTracker;
import org.chromium.ui.edge_to_edge.EdgeToEdgePadAdjuster;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogManager.ModalDialogType;

import java.util.function.Function;

/** Activity for displaying the browsing history manager. */
@NullMarked
public class HistoryActivity extends SnackbarActivity {
    private @Nullable HistoryManager mHistoryManager;
    private @Nullable ManagedBottomSheetController mBottomSheetController;
    private @Nullable ActivityWindowAndroid mWindowAndroid;

    @Override
    public void onActivityResult(int requestCode, int resultCode, @Nullable Intent intent) {
        super.onActivityResult(requestCode, resultCode, intent);
        if (mWindowAndroid != null) {
            assumeNonNull(mWindowAndroid.getIntentRequestTracker())
                    .onActivityResult(requestCode, resultCode, intent);
        }
    }

    @Override
    protected void onProfileAvailable(Profile profile) {
        super.onProfileAvailable(profile);

        boolean appSpecificHistory =
                getIntent().getBooleanExtra(IntentHandler.EXTRA_APP_SPECIFIC_HISTORY, false);
        // For now, we only hide the clear data button for app specific history.
        boolean shouldShowClearData = !appSpecificHistory;
        String clientPackageName =
                IntentUtils.safeGetStringExtra(getIntent(), Intent.EXTRA_PACKAGE_NAME);
        HistoryUmaRecorder historyUmaRecorder =
                appSpecificHistory ? new AppHistoryUmaRecorder() : new HistoryUmaRecorder();
        boolean showAppFilter = !appSpecificHistory && !profile.isOffTheRecord();
        Function<View, EdgeToEdgePadAdjuster> edgeToEdgePadAdjusterGenerator = null;
        if (ChromeFeatureList.sDrawChromePagesEdgeToEdge.isEnabled()) {
            edgeToEdgePadAdjusterGenerator =
                    (view) ->
                            EdgeToEdgeControllerFactory.createForViewAndObserveSupplier(
                                    view, getEdgeToEdgeSupplier());
        }
        mWindowAndroid =
                new ActivityWindowAndroid(
                        this,
                        /* listenToActivityState= */ true,
                        IntentRequestTracker.createFromActivity(this),
                        getInsetObserver(),
                        /* trackOcclusion= */ true);
        mHistoryManager =
                new HistoryManager(
                        profile,
                        mWindowAndroid,
                        this,
                        true,
                        getSnackbarManager(),
                        () -> assertNonNull(mBottomSheetController),
                        getModalDialogManagerSupplier(),
                        getActivityResultTracker(),
                        /* Supplier<@Nullable Tab>= */ null,
                        new BrowsingHistoryBridge(profile.getOriginalProfile()),
                        historyUmaRecorder,
                        clientPackageName,
                        shouldShowClearData,
                        appSpecificHistory,
                        showAppFilter,
                        /* openHistoryItemCallback= */ null,
                        edgeToEdgePadAdjusterGenerator);
        ViewGroup contentView = mHistoryManager.getView();
        setContentView(contentView);
        if (showAppFilter) createBottomSheetController(contentView);
        BackPressHelper.create(this, getOnBackPressedDispatcher(), mHistoryManager);
    }

    private void createBottomSheetController(ViewGroup contentView) {
        ViewGroup sheetContainer =
                (ViewGroup)
                        LayoutInflater.from(this).inflate(R.layout.bottom_sheet_container, null);
        ScrimManager scrimManager =
                new ScrimManager(this, contentView, ScrimClient.HISTORY_ACTIVITY);
        mBottomSheetController =
                BottomSheetControllerFactory.createBottomSheetController(
                        SupplierUtils.of(scrimManager),
                        CallbackUtils.emptyCallback(),
                        getWindow(),
                        assumeNonNull(mWindowAndroid).getKeyboardDelegate(),
                        SupplierUtils.of(sheetContainer),
                        SupplierUtils.of(0),
                        /* desktopWindowStateManager= */ null);

        // HistoryActivity needs its own container for bottom sheet. Add it as a child of the
        // layout enclosing the history list layout so they'll be siblings. HistoryPage doesn't
        // need this since it may share the one from tabbed browser activity.
        contentView.addView(sheetContainer);
    }

    @Override
    protected ModalDialogManager createModalDialogManager() {
        return new ModalDialogManager(new AppModalPresenter(this), ModalDialogType.APP);
    }

    @Override
    protected void onDestroy() {
        if (mHistoryManager != null) {
            mHistoryManager.onDestroyed();
            mHistoryManager = null;
        }
        if (mWindowAndroid != null) {
            mWindowAndroid.destroy();
            mWindowAndroid = null;
        }
        super.onDestroy();
    }

    @Nullable HistoryManager getHistoryManagerForTests() {
        return mHistoryManager;
    }
}
