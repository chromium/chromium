// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management.tab_bottom_sheet;

import android.app.Activity;
import android.view.LayoutInflater;
import android.view.View;

import org.chromium.base.Callback;
import org.chromium.base.CallbackUtils;
import org.chromium.base.supplier.MonotonicObservableSupplier;
import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.omnibox.BackKeyBehaviorDelegate;
import org.chromium.chrome.browser.omnibox.LocationBarCoordinator;
import org.chromium.chrome.browser.omnibox.LocationBarEmbedder;
import org.chromium.chrome.browser.omnibox.LocationBarEmbedderUiOverrides;
import org.chromium.chrome.browser.omnibox.OmniboxActionDelegateImpl;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.ui.base.WindowAndroid;

/** The fusebox (omnibox) component for the tab bottom sheet. */
@NullMarked
public class TabBottomSheetFusebox {
    private final BackKeyBehaviorDelegate mBackKeyBehaviorDelegate =
            new BackKeyBehaviorDelegate() {};
    private final LocationBarCoordinator mLocationBarCoordinator;
    private final View mContentView;
    private final TabBottomSheetFuseboxDataProvider mDataProvider;

    TabBottomSheetFusebox(
            Activity activity,
            MonotonicObservableSupplier<Profile> profileSupplier,
            WindowAndroid windowAndroid,
            ActivityLifecycleDispatcher lifecycleDispatcher,
            Callback<String> loadUrlCallback,
            SnackbarManager snackbarManager) {
        mDataProvider = new TabBottomSheetFuseboxDataProvider();
        mDataProvider.initialize(activity, profileSupplier.get().isOffTheRecord());

        mContentView = LayoutInflater.from(activity).inflate(R.layout.search_activity, null);
        View locationBarLayout = mContentView.findViewById(R.id.search_location_bar);
        View anchorView = mContentView.findViewById(R.id.toolbar);
        View controlContainer = mContentView.findViewById(R.id.control_container);
        View bottomContainer = mContentView.findViewById(R.id.bottom_container);

        LocationBarEmbedderUiOverrides uiOverrides = new LocationBarEmbedderUiOverrides();
        uiOverrides.setForcedPhoneStyleOmnibox();
        uiOverrides.setLensEntrypointAllowed(true);
        uiOverrides.setVoiceEntrypointAllowed(true);

        // LocationBarCoordinator args.
        OmniboxActionDelegateImpl omniboxActionDelegate =
                new OmniboxActionDelegateImpl(
                        activity,
                        () -> null,
                        /* openUrlInExistingTabElseNewTabCb= */ (url) -> {},
                        /* openIncognitoTabCb= */ CallbackUtils.emptyRunnable(),
                        /* openPasswordSettingsCb= */ CallbackUtils.emptyRunnable(),
                        /* openQuickDeleteCb= */ CallbackUtils.emptyRunnable(),
                        /* tabWindowManagerSupplier= */ () -> null,
                        /* bringTabToFrontCallback= */ (tabInfo, url) -> {});

        mLocationBarCoordinator =
                new LocationBarCoordinator(
                        locationBarLayout,
                        anchorView,
                        profileSupplier,
                        mDataProvider,
                        /* actionModeCallback */ null,
                        windowAndroid,
                        /* activityTabSupplier= */ () -> null,
                        windowAndroid::getModalDialogManager,
                        /* shareDelegateSupplier= */ null,
                        /* incognitoStateProvider= */ null,
                        lifecycleDispatcher,
                        (params, isIncognitoBranded) -> {
                            loadUrlCallback.onResult(params.url);
                            return true;
                        },
                        /* backKeyBehavior= */ mBackKeyBehaviorDelegate,
                        /* pageInfoAction= */ (tab, pageInfoHighlight) -> {},
                        /* bringTabGroupToFrontCallback= */ CallbackUtils.emptyCallback(),
                        /* omniboxUma= */ (url, transition, isNtp) -> {},
                        /* bookmarkState= */ (url) -> false,
                        /* isToolbarMicEnabledSupplier= */ () -> true,
                        /* merchantTrustSignalsCoordinatorSupplier= */ null,
                        omniboxActionDelegate,
                        /* browserControlsVisibilityDelegate= */ null,
                        /* backPressManager= */ null,
                        /* omniboxSuggestionsDropdownScrollListener= */ null,
                        /* tabModelSelectorSupplier= */ ObservableSuppliers.alwaysNull(),
                        /* topInsetProviderSupplier= */ ObservableSuppliers.alwaysNull(),
                        new LocationBarEmbedder() {},
                        uiOverrides,
                        controlContainer,
                        /* bottomWindowPaddingSupplier= */ () -> 0,
                        /* onLongClickListener= */ null,
                        /* browserControlsStateProvider= */ null,
                        /* isToolbarPositionCustomizationEnabled= */ false,
                        /* pageZoomManager= */ null,
                        /* tabFaviconFunction= */ (tab) -> null,
                        /* multiInstanceManager= */ null,
                        snackbarManager,
                        bottomContainer);
        mLocationBarCoordinator.setUrlBarFocusable(true);
        mLocationBarCoordinator.onFinishNativeInitialization();
    }

    void destroy() {
        mLocationBarCoordinator.destroy();
    }

    /* Returns the fusebox view */
    View getFuseboxView() {
        return mContentView;
    }

    /* Called when the bottom sheet is shown */
    void onBottomSheetShown() {
        if (mLocationBarCoordinator != null) {
            // When the locationBar is first inflated, it has the width of 0. This causes the
            // locationBar to enter narrow mode, which hides all buttons.
            // This forces the locationBar to remeasure its width and show the required buttons.
            mLocationBarCoordinator.setUrlFocusChangeFraction(1.0f, 1.0f);
        }
    }
}
