// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.app.download.home;

import android.app.Activity;
import android.view.ViewGroup;

import org.chromium.base.Callback;
import org.chromium.base.ThreadUtils;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.back_press.BackPressManager;
import org.chromium.chrome.browser.download.DownloadUtils;
import org.chromium.chrome.browser.download.home.DownloadManagerCoordinator;
import org.chromium.chrome.browser.download.home.DownloadManagerUiConfig;
import org.chromium.chrome.browser.download.home.DownloadManagerUiConfigHelper;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.profiles.OtrProfileId;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.browser.ui.native_page.BasicNativePage;
import org.chromium.chrome.browser.ui.native_page.NativePageHost;
import org.chromium.components.browser_ui.widget.gesture.BackPressHandler;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.ui.modaldialog.ModalDialogManager;

import java.util.function.Supplier;

/** Native page for managing downloads handled through Chrome. */
@NullMarked
public class DownloadPage extends BasicNativePage implements DownloadManagerCoordinator.Observer {
    private DownloadManagerCoordinator mDownloadCoordinator;
    private final String mTitle;

    /**
     * Create a new instance of the downloads page.
     *
     * @param activity The activity to get context and manage fragments.
     * @param snackbarManager The {@link SnackbarManager} to show snack bars.
     * @param modalDialogManager The {@link ModalDialogManager} associated with the activity.
     * @param otrProfileId The {@link OtrProfileId} for the profile. Null for regular profile.
     * @param host A NativePageHost to load urls.
     * @param backPressmanager The {@link BackPressManager} for handling back press.
     */
    public DownloadPage(
            Activity activity,
            SnackbarManager snackbarManager,
            @Nullable ModalDialogManager modalDialogManager,
            @Nullable OtrProfileId otrProfileId,
            NativePageHost host,
            BackPressManager backPressManager) {
        super(host);

        ThreadUtils.assertOnUiThread();

        DownloadManagerUiConfig config =
                DownloadManagerUiConfigHelper.fromFlags(activity)
                        .setOtrProfileId(otrProfileId)
                        .setIsSeparateActivity(false)
                        .setShowPaginationHeaders(DownloadUtils.shouldShowPaginationHeaders())
                        .setEdgeToEdgePadAdjusterGenerator(host::createEdgeToEdgePadAdjuster)
                        .build();

        mDownloadCoordinator =
                DownloadManagerCoordinatorFactoryHelper.create(
                        activity, config, snackbarManager, modalDialogManager);

        mDownloadCoordinator.addObserver(this);
        mTitle = activity.getString(R.string.menu_downloads);

        initWithView(mDownloadCoordinator.getView());

        initializeBackPressHandler(backPressManager);
    }

    @Override
    public String getTitle() {
        return mTitle;
    }

    @Override
    public String getHost() {
        return UrlConstants.DOWNLOADS_HOST;
    }

    @Override
    public void updateForUrl(String url) {
        super.updateForUrl(url);
        mDownloadCoordinator.updateForUrl(url);
    }

    @Override
    public boolean supportsEdgeToEdge() {
        return true;
    }

    @SuppressWarnings("NullAway")
    @Override
    public void destroy() {
        mDownloadCoordinator.removeObserver(this);
        mDownloadCoordinator.destroy();
        mDownloadCoordinator = null;
        super.destroy();
    }

    // DownloadManagerCoordinator.Observer implementation.
    @Override
    public void onUrlChanged(String url) {
        // We want to squash consecutive download home URLs having different filters into the one
        // having the latest filter. This will avoid requiring user to press back button too many
        // times to exit download home. In the event, chrome gets killed or if user navigates away
        // from download home, we still will be able to come back to the latest filter.
        onStateChange(url, true);
    }

    public ViewGroup getListViewForTesting() {
        return mDownloadCoordinator.getListViewForTesting();
    }

    private void initializeBackPressHandler(BackPressManager backPressManager) {
        if (!ChromeFeatureList.isEnabled(
                ChromeFeatureList.ENABLE_ESCAPE_HANDLING_FOR_SECONDARY_ACTIVITIES)) {
            return;
        }

        // Helper function to get an active handler.
        Supplier<@Nullable BackPressHandler> getActiveHandler =
                () -> {
                    if (mDownloadCoordinator == null) return null;
                    for (BackPressHandler handler : mDownloadCoordinator.getBackPressHandlers()) {
                        if (Boolean.TRUE.equals(
                                handler.getHandleBackPressChangedSupplier().get())) {
                            return handler;
                        }
                    }
                    return null;
                };

        final ObservableSupplierImpl<Boolean> combinedSupplier = new ObservableSupplierImpl<>();

        final Callback<Boolean> recalculateState =
                (ignored) -> {
                    combinedSupplier.set(getActiveHandler.get() != null);
                };

        BackPressHandler adapterHandler =
                new BackPressHandler() {
                    @Override
                    public @BackPressResult int handleBackPress() {
                        BackPressHandler activeHandler = getActiveHandler.get();
                        if (activeHandler != null) {
                            return activeHandler.handleBackPress();
                        }
                        return BackPressResult.FAILURE;
                    }

                    @Override
                    public ObservableSupplier<Boolean> getHandleBackPressChangedSupplier() {
                        return combinedSupplier;
                    }
                };

        for (BackPressHandler handler : mDownloadCoordinator.getBackPressHandlers()) {
            handler.getHandleBackPressChangedSupplier().addObserver(recalculateState);
        }

        // The passed value here is ignored. We could technically pass null, but that would trigger
        // the NullAway warning.
        recalculateState.onResult(/* result= */ true);

        setBackPressHandler(adapterHandler, backPressManager);
    }
}
