// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextualsearch;

import static org.chromium.build.NullUtil.assertNonNull;

import android.content.Context;

import androidx.annotation.VisibleForTesting;

import org.jni_zero.CalledByNative;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.base.Callback;
import org.chromium.base.Log;
import org.chromium.base.UserData;
import org.chromium.base.supplier.MonotonicObservableSupplier;
import org.chromium.base.supplier.NullableObservableSupplier;
import org.chromium.base.supplier.OneShotCallback;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.compositor.overlay_panel.OverlayPanel.StateChangeReason;
import org.chromium.chrome.browser.firstrun.FirstRunStatus;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.locale.LocaleManager;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.readaloud.ReadAloudController;
import org.chromium.chrome.browser.readaloud.ReadAloudControllerSupplier;
import org.chromium.chrome.browser.search_engines.TemplateUrlServiceFactory;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.ui.signin.ForcedSigninStatusProvider;
import org.chromium.components.search_engines.TemplateUrlService;
import org.chromium.components.search_engines.TemplateUrlService.TemplateUrlServiceObserver;
import org.chromium.content_public.browser.GestureListenerManager;
import org.chromium.content_public.browser.GestureStateListener;
import org.chromium.content_public.browser.SelectionClient;
import org.chromium.content_public.browser.SelectionPopupController;
import org.chromium.content_public.browser.WebContents;
import org.chromium.net.NetworkChangeNotifier;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.url.GURL;

import java.util.HashMap;
import java.util.Map;

/** Manages the enabling and disabling and gesture listeners for ContextualSearch on a given Tab. */
@NullMarked
public class ContextualSearchTabHelper extends EmptyTabObserver
        implements NetworkChangeNotifier.ConnectionTypeObserver,
                TemplateUrlServiceObserver,
                UserData {
    private static final String TAG = "ContextualSearch";

    private static final Class<ContextualSearchTabHelper> USER_DATA_KEY =
            ContextualSearchTabHelper.class;

    // A map of native helper objects to their Java counterparts allows unlimited scaling in number
    // of tabs.
    private static final Map<Long, ContextualSearchTabHelper> sNativeHelperMap = new HashMap<>();

    /** The Tab that this helper tracks. */
    private final Tab mTab;

    // Device scale factor.
    private final float mPxToDp;

    private @Nullable TemplateUrlService mTemplateUrlService;

    /** The WebContents associated with the Tab which this helper is monitoring, unless detached. */
    private @Nullable WebContents mWebContents;

    /**
     * The {@link ContextualSearchManager} that's managing this tab. This may point to the manager
     * from another activity during reparenting, or be {@code null} during startup.
     */
    private @Nullable ContextualSearchManager mContextualSearchManager;

    /** The GestureListener used for handling events from the current WebContents. */
    private @Nullable GestureStateListener mGestureStateListener;

    /** Manages incoming calls to Smart Select when available, for the current base WebContents. */
    private @Nullable SelectionClientManager mSelectionClientManager;

    /** The pointer to our native C++ implementation. */
    private long mNativeHelper;

    /** Whether the current default search engine is Google. Is {@code null} if not inited. */
    private @Nullable Boolean mIsDefaultSearchEngineGoogle;

    private final Callback<ContextualSearchManager> mManagerCallback;

    /** The ReadAloudController supplier to get the active playback tab supplier when available. */
    private final @Nullable MonotonicObservableSupplier<ReadAloudController>
            mReadAloudControllerSupplier;

    private final Callback<@Nullable Tab> mActivePlaybackTabCallback =
            this::onActivePlaybackTabUpdated;

    /** To listen for when the current tab has an active ReadAloud playback. */
    private @Nullable NullableObservableSupplier<Tab> mReadAloudActivePlaybackTab;

    /**
     * Retrieves the {@link ContextualSearchTabHelper} for the given {@link Tab}, creating it if it
     * doesn't already exist.
     *
     * @param tab The Tab to get the helper for.
     * @return The {@link ContextualSearchTabHelper}, or null if UserDataHost is null.
     */
    public static @Nullable ContextualSearchTabHelper from(Tab tab) {
        if (tab.getUserDataHost() == null || tab.getWebContents() == null) return null;
        ContextualSearchTabHelper helper = get(tab);
        if (helper == null) {
            helper =
                    tab.getUserDataHost()
                            .setUserData(USER_DATA_KEY, new ContextualSearchTabHelper(tab));
        }
        return helper;
    }

    /** Returns the ContextualSearchTabHelper for the given tab if it exists. */
    public static @Nullable ContextualSearchTabHelper get(Tab tab) {
        if (tab.getUserDataHost() == null) return null;
        return tab.getUserDataHost().getUserData(USER_DATA_KEY);
    }

    /**
     * Constructs a Tab helper that can enable and disable Contextual Search based on Tab activity.
     *
     * @param tab The {@link Tab} to track with this helper.
     */
    @VisibleForTesting
    ContextualSearchTabHelper(Tab tab) {
        mTab = tab;
        tab.addObserver(this);
        // Connect to a network, unless under test.
        if (NetworkChangeNotifier.isInitialized()) {
            NetworkChangeNotifier.addConnectionTypeObserver(this);
        }
        float scaleFactor = 1.f;
        Context context = tab != null ? tab.getContext() : null;
        if (context != null) scaleFactor /= context.getResources().getDisplayMetrics().density;
        mPxToDp = scaleFactor;
        mManagerCallback = (ContextualSearchManager manager) -> updateHooksForTab(mTab);
        mReadAloudControllerSupplier = getReadAloudControllerSupplier(tab);
        if (mReadAloudControllerSupplier != null) {
            new OneShotCallback<>(
                    mReadAloudControllerSupplier, this::onReadAloudControllerSupplierReady);
        }
    }

    // ============================================================================================
    // EmptyTabObserver overrides.
    // ============================================================================================

    @Override
    public void onPageLoadStarted(Tab tab, GURL url) {
        updateHooksForTab(tab);
        ContextualSearchManager manager = getContextualSearchManager(tab);
        if (manager != null) manager.onBasePageLoadStarted();
    }

    private void onReadAloudControllerSupplierReady(ReadAloudController readAloudController) {
        if (readAloudController == null) return;
        if (mReadAloudActivePlaybackTab == null) {
            mReadAloudActivePlaybackTab = readAloudController.getActivePlaybackTabSupplier();
        }
        if (mReadAloudActivePlaybackTab != null) {
            mReadAloudActivePlaybackTab.addSyncObserverAndPostIfNonNull(mActivePlaybackTabCallback);
        }
    }

    private void onActivePlaybackTabUpdated(@Nullable Tab tab) {
        updateContextualSearchHooks(mTab.getWebContents());
    }

    @Override
    public void onContentChanged(Tab tab) {
        // Native initialization happens after a page loads or content is changed to ensure profile
        // is initialized.
        Profile profile = tab.getProfile();
        if (mNativeHelper == 0 && tab.getWebContents() != null) {
            mNativeHelper = ContextualSearchTabHelperJni.get().init(profile);
            var oldValue = sNativeHelperMap.put(mNativeHelper, this);
            assert oldValue == null;
        }
        if (profile != null && mTemplateUrlService == null) {
            mTemplateUrlService = TemplateUrlServiceFactory.getForProfile(profile);
            mTemplateUrlService.addObserver(this);
            if (mTemplateUrlService.isLoaded()) onTemplateURLServiceChanged();
        }
        updateHooksForTab(tab);
    }

    @Override
    public void onDestroyed(Tab tab) {
        if (mNativeHelper != 0) {
            ContextualSearchTabHelperJni.get().destroy(mNativeHelper);
            var oldValue = sNativeHelperMap.remove(mNativeHelper);
            assert oldValue == this;
            mNativeHelper = 0;
        }
        if (mTemplateUrlService != null) {
            mTemplateUrlService.removeObserver(this);
        }
        if (NetworkChangeNotifier.isInitialized()) {
            NetworkChangeNotifier.removeConnectionTypeObserver(this);
        }
        if (mReadAloudActivePlaybackTab != null) {
            mReadAloudActivePlaybackTab.removeObserver(mActivePlaybackTabCallback);
        }
        removeContextualSearchHooks(mWebContents);
        mWebContents = null;
        mContextualSearchManager = null;
        mSelectionClientManager = null;
        mGestureStateListener = null;
        MonotonicObservableSupplier<ContextualSearchManager> supplier =
                getContextualSearchManagerSupplier(mTab);
        if (supplier != null) {
            supplier.removeObserver(mManagerCallback);
        }
    }

    @Override
    public void destroy() {
        onDestroyed(mTab);
        mTab.removeObserver(this);
    }

    @Override
    public void onActivityAttachmentChanged(Tab tab, @Nullable WindowAndroid window) {
        if (window != null) {
            updateHooksForTab(tab);
            maybeObserveManagerCreation();
        } else {
            removeContextualSearchHooks(mWebContents);
            mContextualSearchManager = null;
        }
    }

    @Override
    public void onContextMenuShown(Tab tab) {
        ContextualSearchManager manager = getContextualSearchManager(tab);
        if (manager != null) {
            manager.onContextMenuShown();
        }
    }

    // ============================================================================================
    // TemplateUrlServiceObserver overrides.
    // ============================================================================================

    @Override
    public void onTemplateURLServiceChanged() {
        assert mTemplateUrlService != null;
        boolean isDefaultSearchEngineGoogle = mTemplateUrlService.isDefaultSearchEngineGoogle();
        if (mIsDefaultSearchEngineGoogle == null
                || isDefaultSearchEngineGoogle != mIsDefaultSearchEngineGoogle) {
            mIsDefaultSearchEngineGoogle = isDefaultSearchEngineGoogle;
            updateContextualSearchHooks(mWebContents);
        }
    }

    // ============================================================================================
    // NetworkChangeNotifier.ConnectionTypeObserver overrides.
    // ============================================================================================

    @Override
    public void onConnectionTypeChanged(int connectionType) {
        updateContextualSearchHooks(mWebContents);
    }

    // ============================================================================================
    // Private helpers.
    // ============================================================================================

    /**
     * Should be called whenever the Tab's WebContents may have changed. Removes hooks from the
     * existing WebContents, if necessary, and then adds hooks for the new WebContents.
     * @param tab The current tab.
     */
    private void updateHooksForTab(Tab tab) {
        WebContents currentWebContents = tab.getWebContents();
        boolean webContentsChanged = currentWebContents != mWebContents;
        if (webContentsChanged || mContextualSearchManager != getContextualSearchManager(tab)) {
            mContextualSearchManager = getContextualSearchManager(tab);
            if (webContentsChanged) {
                // Ensure the hooks are cleared on the old web contents before proceeding. All of
                // the objects associated with the web content need to be recreated in order for
                // selection to continue working. See https://crbug.com/40688159 for more details.
                removeContextualSearchHooks(mWebContents);
                mSelectionClientManager =
                        currentWebContents != null
                                ? new SelectionClientManager(currentWebContents)
                                : null;
            }
            mWebContents = currentWebContents;
            updateContextualSearchHooks(mWebContents);
        }
    }

    /**
     * Updates the Contextual Search hooks, adding or removing them depending on whether it is
     * currently active. If the current tab's {@link WebContents} may have changed, call {@link
     * #updateHooksForTab(Tab)} instead.
     *
     * @param webContents The WebContents to attach the gesture state listener to.
     */
    private void updateContextualSearchHooks(@Nullable WebContents webContents) {
        if (webContents == null) return;

        removeContextualSearchHooks(webContents);
        if (isContextualSearchActive(webContents)) addContextualSearchHooks(webContents);
    }

    /**
     * Adds Contextual Search hooks for its client and listener to the given WebContents.
     *
     * @param webContents The WebContents to attach the gesture state listener to.
     */
    private void addContextualSearchHooks(WebContents webContents) {
        assert mTab.getWebContents() == null || mTab.getWebContents() == webContents;
        ContextualSearchManager manager = getContextualSearchManager(mTab);
        if (mGestureStateListener == null && manager != null) {
            mGestureStateListener = manager.getGestureStateListener();
            GestureListenerManager gestureListenerManager =
                    GestureListenerManager.fromWebContents(webContents);
            assert mGestureStateListener != null;
            assert mSelectionClientManager != null;
            assert gestureListenerManager != null;
            gestureListenerManager.addListener(mGestureStateListener);

            // If we needed to add our listener, we also need to add our selection client.
            SelectionPopupController controller =
                    SelectionPopupController.fromWebContents(webContents);
            controller.setSelectionClient(
                    mSelectionClientManager.addContextualSearchSelectionClient(
                            manager.getContextualSearchSelectionClient()));
            ContextualSearchTabHelperJni.get()
                    .installUnhandledTapNotifierIfNeeded(mNativeHelper, webContents, mPxToDp);
        }
    }

    /**
     * Removes Contextual Search hooks for its client and listener from the given WebContents.
     * @param webContents The WebContents to detach the gesture state listener from.
     */
    private void removeContextualSearchHooks(@Nullable WebContents webContents) {
        if (webContents == null) return;

        if (mGestureStateListener != null) {
            GestureListenerManager gestureListenerManager =
                    GestureListenerManager.fromWebContents(webContents);
            // May be null if the WebContents is already destroyed.
            if (gestureListenerManager != null) {
                gestureListenerManager.removeListener(mGestureStateListener);
                mGestureStateListener = null;

                // If we needed to remove our listener, we also need to remove our selection client.
                if (mSelectionClientManager != null) {
                    SelectionPopupController controller =
                            SelectionPopupController.fromWebContents(webContents);
                    SelectionClient client =
                            mSelectionClientManager.removeContextualSearchSelectionClient();

                    if (controller.getSelectionClient()
                            == mSelectionClientManager.getSelectionClient()) {
                        controller.setSelectionClient(client);
                    }
                }
            }
            // Also make sure the UI is hidden if the device is offline.
            ContextualSearchManager manager = getContextualSearchManager(mTab);
            if (manager != null && !isDeviceOnline(manager)) {
                manager.hideContextualSearch(StateChangeReason.UNKNOWN);
            }
        }
    }

    /**
     * @return whether Contextual Search is enabled and active in this tab.
     */
    private boolean isContextualSearchActive(WebContents webContents) {
        assert mTab.getWebContents() == null || mTab.getWebContents() == webContents;
        // If the tab has an active ReadAloud playback, contextual search is disabled
        if (mReadAloudActivePlaybackTab != null && mReadAloudActivePlaybackTab.get() == mTab) {
            return false;
        }
        if (maybeObserveManagerCreation()) return false;

        ContextualSearchManager manager = getContextualSearchManager(mTab);
        assert manager != null;

        Profile profile = Profile.fromWebContents(webContents);
        boolean isDseGoogle =
                TemplateUrlServiceFactory.getForProfile(profile).isDefaultSearchEngineGoogle();
        boolean isActive =
                !webContents.isIncognito()
                        && FirstRunStatus.getFirstRunFlowComplete()
                        && !ForcedSigninStatusProvider.getForProfile(profile)
                                .isForcedSigninShowing()
                        && !ContextualSearchPolicy.isContextualSearchDisabled(profile)
                        && isDseGoogle
                        && !LocaleManager.getInstance().needToCheckForSearchEnginePromo()
                        // Svelte and Accessibility devices are incompatible with the first-run flow
                        // and Talkback has poor interaction with Contextual Search (see
                        // http://crbug.com/40377520 and http://crbug.com/40376140).
                        && !manager.isRunningInCompatibilityMode()
                        && !mTab.isShowingErrorPage()
                        && isDeviceOnline(manager);
        if (mTab.isCustomTab() && !isActive) {
            // TODO(donnd): remove after https://crbug.com/40757075 is resolved.
            Log.w(TAG, "Not allowed to be active! Checking reasons:");
            Log.w(
                    TAG,
                    "!isIncognito: "
                            + !webContents.isIncognito()
                            + " getFirstRunFlowComplete: "
                            + FirstRunStatus.getFirstRunFlowComplete()
                            + "isForcedSigninShowing: "
                            + ForcedSigninStatusProvider.getForProfile(profile)
                                    .isForcedSigninShowing()
                            + " !isContextualSearchDisabled: "
                            + !ContextualSearchManager.isContextualSearchDisabled(profile)
                            + " isDefaultSearchEngineGoogle: "
                            + isDseGoogle
                            + " !needToCheckForSearchEnginePromo: "
                            + !LocaleManager.getInstance().needToCheckForSearchEnginePromo()
                            + " !isRunningInCompatibilityMode: "
                            + !manager.isRunningInCompatibilityMode()
                            + " !isShowingErrorPage: "
                            + !mTab.isShowingErrorPage()
                            + " isDeviceOnline: "
                            + isDeviceOnline(manager));
        }
        return isActive;
    }

    /**
     * Observe {@link ContextualSearchManager} creation if not available.
     *
     * @return {@code True} if the observer is installed. {@code false} if manager already exists,
     *     thus no observer was installed.
     */
    private boolean maybeObserveManagerCreation() {
        ContextualSearchManager manager = getContextualSearchManager(mTab);
        if (manager != null) return false;

        if (mTab.isCustomTab()) Log.w(TAG, "No manager!");
        MonotonicObservableSupplier<ContextualSearchManager> supplier =
                getContextualSearchManagerSupplier(mTab);
        if (supplier != null) {
            supplier.addSyncObserverAndPostIfNonNull(mManagerCallback);
        }
        return true;
    }

    /**
     * @return Whether the device is online, or we have disabled online-detection.
     */
    private boolean isDeviceOnline(ContextualSearchManager manager) {
        return ChromeFeatureList.isEnabled(
                        ChromeFeatureList.CONTEXTUAL_SEARCH_DISABLE_ONLINE_DETECTION)
                ? true
                : manager.isDeviceOnline();
    }

    /**
     * Gets the {@link ContextualSearchManager} associated with the given tab's activity.
     *
     * @param tab The {@link Tab} that we're getting the manager for.
     * @return The Contextual Search manager controlling that Tab.
     */
    private @Nullable ContextualSearchManager getContextualSearchManager(Tab tab) {
        var supplier = getContextualSearchManagerSupplier(tab);
        return supplier != null ? supplier.get() : null;
    }

    private @Nullable MonotonicObservableSupplier<ContextualSearchManager>
            getContextualSearchManagerSupplier(Tab tab) {
        // Window may be null in tests.
        WindowAndroid window = tab.getWindowAndroid();
        return window != null ? ContextualSearchManagerSupplier.from(window) : null;
    }

    private static @Nullable MonotonicObservableSupplier<ReadAloudController>
            getReadAloudControllerSupplier(Tab tab) {
        // Window may be null in tests.
        WindowAndroid window = tab.getWindowAndroid();
        return window != null ? ReadAloudControllerSupplier.from(window) : null;
    }

    // ============================================================================================
    // Native support.
    // ============================================================================================

    @CalledByNative
    void onContextualSearchPrefChanged() {
        updateContextualSearchHooks(mWebContents);

        ContextualSearchManager manager = getContextualSearchManager(mTab);
        if (manager != null) {
            manager.onContextualSearchPrefChanged();
        }
    }

    /**
     * Notifies this helper to show the Unhandled Tap UI due to a tap at the given pixel
     * coordinates.
     */
    @CalledByNative
    void onShowUnhandledTapUiIfNeeded(int x, int y) {
        // Only notify the manager if we currently have a valid listener.
        ContextualSearchManager manager = getContextualSearchManager(mTab);
        if (mGestureStateListener != null && manager != null) {
            manager.onShowUnhandledTapUiIfNeeded(x, y);
        }
    }

    @CalledByNative
    private static ContextualSearchTabHelper getJavaObject(long nativeHelper) {
        return assertNonNull(sNativeHelperMap.get(nativeHelper));
    }

    @NativeMethods
    interface Natives {
        long init(@JniType("Profile*") Profile profile);

        void installUnhandledTapNotifierIfNeeded(
                long nativeContextualSearchTabHelper,
                WebContents webContents,
                float pxToDpScaleFactor);

        void destroy(long nativeContextualSearchTabHelper);
    }
}
