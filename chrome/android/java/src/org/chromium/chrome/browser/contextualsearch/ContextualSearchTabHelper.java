// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextualsearch;

import android.content.Context;

import androidx.annotation.Nullable;

import org.jni_zero.CalledByNative;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.base.Callback;
import org.chromium.base.Log;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.OneShotCallback;
import org.chromium.chrome.browser.compositor.bottombar.OverlayPanel.StateChangeReason;
import org.chromium.chrome.browser.firstrun.FirstRunStatus;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.locale.LocaleManager;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.readaloud.ReadAloudController;
import org.chromium.chrome.browser.readaloud.ReadAloudControllerSupplier;
import org.chromium.chrome.browser.search_engines.TemplateUrlServiceFactory;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
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

/** Manages the enabling and disabling and gesture listeners for ContextualSearch on a given Tab. */
public class ContextualSearchTabHelper extends EmptyTabObserver
        implements NetworkChangeNotifier.ConnectionTypeObserver, TemplateUrlServiceObserver {
    private static final String TAG = "ContextualSearch";

    /** The Tab that this helper tracks. */
    private final Tab mTab;

    // Device scale factor.
    private final float mPxToDp;

    private TemplateUrlService mTemplateUrlService;

    /** The WebContents associated with the Tab which this helper is monitoring, unless detached. */
    private WebContents mWebContents;

    /**
     * The {@link ContextualSearchManager} that's managing this tab. This may point to
     * the manager from another activity during reparenting, or be {@code null} during startup.
     */
    private ContextualSearchManager mContextualSearchManager;

    /** The GestureListener used for handling events from the current WebContents. */
    private GestureStateListener mGestureStateListener;

    /** Manages incoming calls to Smart Select when available, for the current base WebContents. */
    private SelectionClientManager mSelectionClientManager;

    /** The pointer to our native C++ implementation. */
    private long mNativeHelper;

    /** Whether the current default search engine is Google.  Is {@code null} if not inited. */
    private Boolean mIsDefaultSearchEngineGoogle;

    private Callback<ContextualSearchManager> mManagerCallback;

    /** The ReadAloudController supplier to get the active playback tab supplier when available. */
    private ObservableSupplier<ReadAloudController> mReadAloudControllerSupplier;

    /** To listen for when the current tab has an active ReadAloud playback. */
    private ObservableSupplier<Tab> mReadAloudActivePlaybackTab;

    /** Callback for when the ReadAloudController is ready. */
    private OneShotCallback<ReadAloudController> mReadAloudControllerSupplierCallback;

    /**
     * Creates a contextual search tab helper for the given tab.
     *
     * @param tab The tab whose contextual search actions will be handled by this helper.
     */
    public static void createForTab(Tab tab) {
        new ContextualSearchTabHelper(tab);
    }

    /**
     * Constructs a Tab helper that can enable and disable Contextual Search based on Tab activity.
     * @param tab The {@link Tab} to track with this helper.
     */
    private ContextualSearchTabHelper(Tab tab) {
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
        if (isReadAloudTapToSeekEnabled()) {
            mReadAloudControllerSupplier = getReadAloudControllerSupplier(tab);
            if (mReadAloudControllerSupplier != null) {
                mReadAloudControllerSupplierCallback =
                        new OneShotCallback<ReadAloudController>(
                                mReadAloudControllerSupplier,
                                this::onReadAloudControllerSupplierReady);
            }
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
            mReadAloudActivePlaybackTab.addObserver(this::onActivePlaybackTabUpdated);
        }
    }

    private void onActivePlaybackTabUpdated(Tab tab) {
        updateContextualSearchHooks(mTab.getWebContents());
    }

    @Override
    public void onContentChanged(Tab tab) {
        // Native initialization happens after a page loads or content is changed to ensure profile
        // is initialized.
        Profile profile = tab.getProfile();
        if (mNativeHelper == 0 && tab.getWebContents() != null) {
            mNativeHelper =
                    ContextualSearchTabHelperJni.get()
                            .init(ContextualSearchTabHelper.this, profile);
        }
        if (profile != null && mTemplateUrlService == null) {
            mTemplateUrlService = TemplateUrlServiceFactory.getForProfile(profile);
            mTemplateUrlService.addObserver(this);
            if (mTemplateUrlService.isLoaded()) onTemplateURLServiceChanged();
        }
        updateHooksForTab(tab);
    }

    @Override
    public void onWebContentsSwapped(Tab tab, boolean didStartLoad, boolean didFinishLoad) {
        updateHooksForTab(tab);
    }

    @Override
    public void onDestroyed(Tab tab) {
        if (mNativeHelper != 0) {
            ContextualSearchTabHelperJni.get()
                    .destroy(mNativeHelper, ContextualSearchTabHelper.this);
            mNativeHelper = 0;
        }
        if (mTemplateUrlService != null) {
            mTemplateUrlService.removeObserver(this);
        }
        if (NetworkChangeNotifier.isInitialized()) {
            NetworkChangeNotifier.removeConnectionTypeObserver(this);
        }
        removeContextualSearchHooks(mWebContents);
        mWebContents = null;
        mContextualSearchManager = null;
        mSelectionClientManager = null;
        mGestureStateListener = null;
        ObservableSupplier<ContextualSearchManager> supplier =
                getContextualSearchManagerSupplier(mTab);
        if (supplier != null) {
            supplier.removeObserver(mManagerCallback);
        }
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
                // selection to continue working. See https://crbug.com/1076326 for more details.
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
    private void updateContextualSearchHooks(WebContents webContents) {
        if (webContents == null) return;

        removeContextualSearchHooks(webContents);
        if (isContextualSearchActive(webContents)) addContextualSearchHooks(webContents);
    }

    /**
     * Adds Contextual Search hooks for its client and listener to the given WebContents.
     * @param webContents The WebContents to attach the gesture state listener to.
     */
    private void addContextualSearchHooks(WebContents webContents) {
        assert mTab.getWebContents() == null || mTab.getWebContents() == webContents;
        ContextualSearchManager contextualSearchManager = getContextualSearchManager(mTab);
        if (mGestureStateListener == null && contextualSearchManager != null) {
            mGestureStateListener = contextualSearchManager.getGestureStateListener();
            GestureListenerManager.fromWebContents(webContents).addListener(mGestureStateListener);

            // If we needed to add our listener, we also need to add our selection client.
            SelectionPopupController controller =
                    SelectionPopupController.fromWebContents(webContents);
            controller.setSelectionClient(
                    mSelectionClientManager.addContextualSearchSelectionClient(
                            contextualSearchManager.getContextualSearchSelectionClient()));
            ContextualSearchTabHelperJni.get()
                    .installUnhandledTapNotifierIfNeeded(
                            mNativeHelper, ContextualSearchTabHelper.this, webContents, mPxToDp);
        }
    }

    /**
     * Removes Contextual Search hooks for its client and listener from the given WebContents.
     * @param webContents The WebContents to detach the gesture state listener from.
     */
    private void removeContextualSearchHooks(@Nullable WebContents webContents) {
        if (webContents == null) return;

        if (mGestureStateListener != null) {
            GestureListenerManager.fromWebContents(webContents)
                    .removeListener(mGestureStateListener);
            mGestureStateListener = null;

            // If we needed to remove our listener, we also need to remove our selection client.
            if (mSelectionClientManager != null) {
                SelectionPopupController controller =
                        SelectionPopupController.fromWebContents(webContents);
                SelectionClient client =
                        mSelectionClientManager.removeContextualSearchSelectionClient();
                if (isReadAloudTapToSeekEnabled()) {
                    if (controller.getSelectionClient()
                            == mSelectionClientManager.getSelectionClient()) {
                        controller.setSelectionClient(client);
                    }
                } else {
                    controller.setSelectionClient(client);
                }
            }
            // Also make sure the UI is hidden if the device is offline.
            ContextualSearchManager contextualSearchManager = getContextualSearchManager(mTab);
            if (contextualSearchManager != null && !isDeviceOnline(contextualSearchManager)) {
                contextualSearchManager.hideContextualSearch(StateChangeReason.UNKNOWN);
            }
        }
    }

    /**
     * @return whether Contextual Search is enabled and active in this tab.
     */
    private boolean isContextualSearchActive(WebContents webContents) {
        assert mTab.getWebContents() == null || mTab.getWebContents() == webContents;
        // If the tab has an active ReadAloud playback, contextual search is disabled
        if (isReadAloudTapToSeekEnabled()
                && mReadAloudActivePlaybackTab != null
                && mReadAloudActivePlaybackTab.get() == mTab
                && mTab != null) {
            return false;
        }
        if (maybeObserveManagerCreation()) return false;

        ContextualSearchManager manager = getContextualSearchManager(mTab);
        Profile profile = Profile.fromWebContents(webContents);
        boolean isDseGoogle =
                TemplateUrlServiceFactory.getForProfile(profile).isDefaultSearchEngineGoogle();
        boolean isActive =
                !webContents.isIncognito()
                        && FirstRunStatus.getFirstRunFlowComplete()
                        && !ContextualSearchPolicy.isContextualSearchDisabled(profile)
                        && isDseGoogle
                        && !LocaleManager.getInstance().needToCheckForSearchEnginePromo()
                        // Svelte and Accessibility devices are incompatible with the first-run flow
                        // and Talkback has poor interaction with Contextual Search (see
                        // http://crbug.com/399708 and http://crbug.com/396934).
                        && !manager.isRunningInCompatibilityMode()
                        && !mTab.isShowingErrorPage()
                        && isDeviceOnline(manager);
        if (mTab.isCustomTab() && !isActive) {
            // TODO(donnd): remove after https://crbug.com/1192143 is resolved.
            Log.w(TAG, "Not allowed to be active! Checking reasons:");
            Log.w(
                    TAG,
                    "!isIncognito: "
                            + !webContents.isIncognito()
                            + " getFirstRunFlowComplete: "
                            + FirstRunStatus.getFirstRunFlowComplete()
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
        ObservableSupplier<ContextualSearchManager> supplier =
                getContextualSearchManagerSupplier(mTab);
        if (supplier != null) {
            supplier.addObserver(mManagerCallback);
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
     * @return Whether ReadAloud's tap to seek is enabled
     */
    private static boolean isReadAloudTapToSeekEnabled() {
        return ChromeFeatureList.sReadAloudTapToSeek.isEnabled();
    }

    /**
     * Gets the {@link ContextualSearchManager} associated with the given tab's activity.
     *
     * @param tab The {@link Tab} that we're getting the manager for.
     * @return The Contextual Search manager controlling that Tab.
     */
    private ContextualSearchManager getContextualSearchManager(Tab tab) {
        var supplier = getContextualSearchManagerSupplier(tab);
        return supplier != null ? supplier.get() : null;
    }

    private ObservableSupplier<ContextualSearchManager> getContextualSearchManagerSupplier(
            Tab tab) {
        // Window may be null in tests.
        WindowAndroid window = tab.getWindowAndroid();
        return window != null ? ContextualSearchManagerSupplier.from(window) : null;
    }

    private static ObservableSupplier<ReadAloudController> getReadAloudControllerSupplier(Tab tab) {
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
    void onShowUnhandledTapUIIfNeeded(int x, int y) {
        // Only notify the manager if we currently have a valid listener.
        if (mGestureStateListener != null && getContextualSearchManager(mTab) != null) {
            getContextualSearchManager(mTab).onShowUnhandledTapUIIfNeeded(x, y);
        }
    }

    @NativeMethods
    interface Natives {
        long init(ContextualSearchTabHelper caller, @JniType("Profile*") Profile profile);

        void installUnhandledTapNotifierIfNeeded(
                long nativeContextualSearchTabHelper,
                ContextualSearchTabHelper caller,
                WebContents webContents,
                float pxToDpScaleFactor);

        void destroy(long nativeContextualSearchTabHelper, ContextualSearchTabHelper caller);
    }
}
