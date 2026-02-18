// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_bottom_sheet;

import android.app.Activity;
import android.view.View;

import org.chromium.base.Callback;
import org.chromium.base.CallbackUtils;
import org.chromium.base.supplier.NonNullObservableSupplier;
import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.omnibox.BackKeyBehaviorDelegate;
import org.chromium.chrome.browser.omnibox.LocationBarCoordinator;
import org.chromium.chrome.browser.omnibox.LocationBarEmbedder;
import org.chromium.chrome.browser.omnibox.LocationBarEmbedderUiOverrides;
import org.chromium.chrome.browser.omnibox.suggestions.action.OmniboxActionDelegateImpl;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.ui.edge_to_edge.NoOpTopInsetProvider;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.ui.base.WindowAndroid;

/** The fusebox (omnibox) component for the tab bottom sheet. */
@NullMarked
public class TabBottomSheetFusebox {
    /** Delegate for the tab bottom sheet fusebox. */
    public static class TabBottomSheetFuseboxConfig {
        public final View contentView;
        public final View locationBarLayout;
        public final View anchorView;
        public final View controlContainer;
        public final View bottomContainer;
        public final OmniboxActionDelegateImpl omniboxActionDelegate;

        public TabBottomSheetFuseboxConfig(
                View contentView,
                View locationBarLayout,
                View anchorView,
                View controlContainer,
                View bottomContainer,
                OmniboxActionDelegateImpl omniboxActionDelegate) {
            this.contentView = contentView;
            this.locationBarLayout = locationBarLayout;
            this.anchorView = anchorView;
            this.controlContainer = controlContainer;
            this.bottomContainer = bottomContainer;
            this.omniboxActionDelegate = omniboxActionDelegate;
        }
    }

    private final BackKeyBehaviorDelegate mBackKeyBehaviorDelegate =
            new BackKeyBehaviorDelegate() {};
    private final LocationBarCoordinator mLocationBarCoordinator;
    private final View mContentView;
    private final TabBottomSheetFuseboxDataProvider mDataProvider;

    TabBottomSheetFusebox(
            Activity activity,
            TabBottomSheetFuseboxConfig config,
            NonNullObservableSupplier<Profile> profileSupplier,
            WindowAndroid windowAndroid,
            ActivityLifecycleDispatcher lifecycleDispatcher,
            Callback<String> loadUrlCallback,
            SnackbarManager snackbarManager) {

        mDataProvider = new TabBottomSheetFuseboxDataProvider();
        mDataProvider.initialize(activity, profileSupplier.get().isOffTheRecord());

        mContentView = config.contentView;
        View locationBarLayout = config.locationBarLayout;
        View anchorView = config.anchorView;
        View controlContainer = config.controlContainer;
        View bottomContainer = config.bottomContainer;

        LocationBarEmbedderUiOverrides uiOverrides = new LocationBarEmbedderUiOverrides();
        uiOverrides.setForcedPhoneStyleOmnibox();
        uiOverrides.setLensEntrypointAllowed(true);
        uiOverrides.setVoiceEntrypointAllowed(true);

        mLocationBarCoordinator =
                new LocationBarCoordinator(
                        locationBarLayout,
                        anchorView,
                        profileSupplier,
                        mDataProvider,
                        /* actionModeCallback */ null,
                        windowAndroid,
                        /* activityTabSupplier= */ ObservableSuppliers.alwaysNull(),
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
                        config.omniboxActionDelegate,
                        /* browserControlsVisibilityDelegate= */ null,
                        /* backPressManager= */ null,
                        /* omniboxSuggestionsDropdownScrollListener= */ null,
                        /* tabModelSelectorSupplier= */ ObservableSuppliers.alwaysNull(),
                        /* topInsetProvider= */ new NoOpTopInsetProvider(),
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
                        bottomContainer,
                        /* omniboxChipManager= */ null);
        mLocationBarCoordinator.setUrlBarFocusable(true);
        mLocationBarCoordinator.setShouldShowMicButtonWhenUnfocused(true);
        mLocationBarCoordinator.setShouldShowLensButtonWhenUnfocused(true);
        mLocationBarCoordinator.onFinishNativeInitialization();
    }

    void destroy() {
        mLocationBarCoordinator.destroy();
        mDataProvider.destroy();
    }

    /* Returns the fusebox view */
    View getFuseboxView() {
        return mContentView;
    }
}
