// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextualsearch;

import android.app.Activity;
import android.view.ContextMenu;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.compositor.CompositorViewHolder;
import org.chromium.chrome.browser.compositor.bottombar.OverlayPanel.StateChangeReason;
import org.chromium.chrome.browser.compositor.bottombar.OverlayPanelManager.OverlayPanelManagerObserver;
import org.chromium.chrome.browser.compositor.layouts.LayoutManager;
import org.chromium.chrome.browser.contextualsearch.ContextualSearchFieldTrial.ContextualSearchSwitch;
import org.chromium.chrome.browser.firstrun.FirstRunStatus;
import org.chromium.chrome.browser.fullscreen.FullscreenOptions;
import org.chromium.chrome.browser.locale.LocaleManager;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.search_engines.TemplateUrlServiceFactory;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.Tab.TabHidingType;
import org.chromium.chrome.browser.tabmodel.TabSelectionType;
import org.chromium.components.search_engines.TemplateUrlService.TemplateUrlServiceObserver;
import org.chromium.content_public.browser.GestureListenerManager;
import org.chromium.content_public.browser.GestureStateListener;
import org.chromium.content_public.browser.SelectionPopupController;
import org.chromium.content_public.browser.WebContents;
import org.chromium.net.NetworkChangeNotifier;

/** Manages the activation and gesture listeners for ContextualSearch on a given tab. */
public class ContextualSearchTabHelper
        extends EmptyTabObserver implements NetworkChangeNotifier.ConnectionTypeObserver {
    /** The Tab that this helper tracks. */
    private final Tab mTab;

    // Device scale factor.
    private final float mPxToDp;

    /** Notification handler for Contextual Search events. */
    private TemplateUrlServiceObserver mTemplateUrlObserver;

    /**
     * The WebContents associated with the Tab which this helper is monitoring, unless detached.
     */
    private WebContents mWebContents;

    /**
     * The {@link ContextualSearchManager} that's managing this tab. This may point to
     * the manager from another activity during reparenting, or be {@code null} during startup.
     */
    private ContextualSearchManager mContextualSearchManager;

    /** The GestureListener used for handling events from the current WebContents. */
    private GestureStateListener mGestureStateListener;

    /**
     * Manages incoming calls to Smart Select when available, for the current base WebContents.
     */
    private SelectionClientManager mSelectionClientManager;

    /** The pointer to our native C++ implementation. */
    private long mNativeHelper;

    /** {@code true} while observing other overlay panel via {@link OverlayPanelManagerObserver} */
    private boolean mIsObservingPanel;

    /**
     * A Tab that has had Contextual Search unhooked from itself because another overlay is
     * showing on it, or {@code null}.
     */
    private Tab mUnhookedTab;

    /** Whether the current default search engine is Google.  Is {@code null} if not inited. */
    private Boolean mIsDefaultSearchEngineGoogle;

    /**
     * Creates a contextual search tab helper for the given tab.
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
        if (tab != null && tab.getActivity() != null && tab.getActivity().getResources() != null) {
            scaleFactor /= tab.getActivity().getResources().getDisplayMetrics().density;
        }
        mPxToDp = scaleFactor;
    }

    /**
     * Used to disable contextual search (remove contextual search hooks) when other overlay
     * panel comes into action.
     */
    private OverlayPanelManagerObserver mPanelObserver = new OverlayPanelManagerObserver() {
        @Override
        public void onOverlayPanelShown() {
            // This leaves the handling of the hooks to the responsibility of the activity tab.
            // Restoring them will be then done by the tab that was the activity tab when
            // the panel was shown.
            Tab activityTab = mTab.getActivity().getActivityTabProvider().get();
            if (activityTab != mTab) return;

            // Removes the hooks if the panel other than contextual search panel just got shown.
            ContextualSearchManager manager = getContextualSearchManager(mTab);
            if (manager != null && !manager.isSearchPanelActive()) {
                mUnhookedTab = activityTab;
                updateContextualSearchHooks(mUnhookedTab.getWebContents());
            }
        }

        @Override
        public void onOverlayPanelHidden() {
            if (mUnhookedTab != null) {
                WebContents webContents = mUnhookedTab.getWebContents();
                mUnhookedTab = null;
                updateContextualSearchHooks(webContents);
            }
        }
    };

    /**
     * Starts observing other panel using {@link OverlayPanelManagerObserver} if we're not
     * already doing it.
     * @param tab {@link Tab} to get the overlay panel manager to add the observer to.
     */
    private void addPanelObserver(Tab tab) {
        if (mIsObservingPanel || tab.isNativePage()) return;
        LayoutManager manager = getLayoutManager(tab);
        if (manager != null) {
            manager.getOverlayPanelManager().addObserver(mPanelObserver);
            mIsObservingPanel = true;
        }
    }

    /**
     * Stops observing other panel if we haven't stopped it already.
     * @param tab {@link Tab} to get the overlay panel manager to remove the observer from.
     */
    private void removePanelObserver(Tab tab) {
        if (!mIsObservingPanel || tab.isNativePage()) return;
        LayoutManager manager = getLayoutManager(tab);
        if (manager != null) {
            manager.getOverlayPanelManager().removeObserver(mPanelObserver);
            mIsObservingPanel = false;
        }
    }

    private static LayoutManager getLayoutManager(Tab tab) {
        if (tab.getActivity() == null) return null;
        CompositorViewHolder cvh = tab.getActivity().getCompositorViewHolder();
        return cvh != null ? cvh.getLayoutManager() : null;
    }

    // ============================================================================================
    // EmptyTabObserver overrides.
    // ============================================================================================

    @Override
    public void onPageLoadStarted(Tab tab, String url) {
        updateHooksForTab(tab);
        ContextualSearchManager manager = getContextualSearchManager(tab);
        if (manager != null) manager.onBasePageLoadStarted();
    }

    @Override
    public void onPageLoadFinished(Tab tab, String url) {
        // Makes sure the observer is added. Doing this in |onShown| doesn't cover all
        // situations as it can be invoked before OverlayPanelManager is ready.
        addPanelObserver(tab);
    }

    @Override
    public void onShown(Tab tab, @TabSelectionType int type) {
        addPanelObserver(tab);
    }

    @Override
    public void onHidden(Tab tab, @TabHidingType int type) {
        removePanelObserver(tab);
    }

    @Override
    public void onContentChanged(Tab tab) {
        // Native initialization happens after a page loads or content is changed to ensure profile
        // is initialized.
        if (mNativeHelper == 0) {
            mNativeHelper = ContextualSearchTabHelperJni.get().init(
                    ContextualSearchTabHelper.this, tab.getProfile());
        }
        if (mTemplateUrlObserver == null) {
            mTemplateUrlObserver = new TemplateUrlServiceObserver() {
                @Override
                public void onTemplateURLServiceChanged() {
                    boolean isDefaultSearchEngineGoogle =
                            TemplateUrlServiceFactory.get().isDefaultSearchEngineGoogle();
                    if (mIsDefaultSearchEngineGoogle == null
                            || isDefaultSearchEngineGoogle != mIsDefaultSearchEngineGoogle) {
                        mIsDefaultSearchEngineGoogle = isDefaultSearchEngineGoogle;
                        updateContextualSearchHooks(mWebContents);
                    }
                }
            };
            TemplateUrlServiceFactory.get().addObserver(mTemplateUrlObserver);
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
            ContextualSearchTabHelperJni.get().destroy(
                    mNativeHelper, ContextualSearchTabHelper.this);
            mNativeHelper = 0;
        }
        if (mTemplateUrlObserver != null) {
            TemplateUrlServiceFactory.get().removeObserver(mTemplateUrlObserver);
        }
        if (NetworkChangeNotifier.isInitialized()) {
            NetworkChangeNotifier.removeConnectionTypeObserver(this);
        }
        removePanelObserver(tab);
        removeContextualSearchHooks(mWebContents);
        mWebContents = null;
        mContextualSearchManager = null;
        mSelectionClientManager = null;
        mGestureStateListener = null;
    }

    @Override
    public void onEnterFullscreenMode(Tab tab, FullscreenOptions options) {
        ContextualSearchManager manager = getContextualSearchManager(tab);
        if (manager != null) {
            manager.hideContextualSearch(StateChangeReason.UNKNOWN);
        }
    }

    @Override
    public void onExitFullscreenMode(Tab tab) {
        ContextualSearchManager manager = getContextualSearchManager(tab);
        if (manager != null) {
            manager.hideContextualSearch(StateChangeReason.UNKNOWN);
        }
    }

    @Override
    public void onActivityAttachmentChanged(Tab tab, boolean isAttached) {
        if (isAttached) {
            updateHooksForTab(tab);
            addPanelObserver(tab);
        } else {
            removeContextualSearchHooks(mWebContents);
            removePanelObserver(tab);
            mContextualSearchManager = null;
        }
    }

    @Override
    public void onContextMenuShown(Tab tab, ContextMenu menu) {
        ContextualSearchManager manager = getContextualSearchManager(tab);
        if (manager != null) {
            manager.onContextMenuShown();
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
        if (currentWebContents != mWebContents
                || mContextualSearchManager != getContextualSearchManager(tab)) {
            mWebContents = currentWebContents;
            mContextualSearchManager = getContextualSearchManager(tab);
            if (mWebContents != null && mSelectionClientManager == null) {
                mSelectionClientManager = new SelectionClientManager(mWebContents);
            }
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
            ContextualSearchTabHelperJni.get().installUnhandledTapNotifierIfNeeded(
                    mNativeHelper, ContextualSearchTabHelper.this, webContents, mPxToDp);
        }
    }

    /**
     * Removes Contextual Search hooks for its client and listener from the given WebContents.
     * @param webContents The WebContents to detach the gesture state listener from.
     */
    private void removeContextualSearchHooks(WebContents webContents) {
        if (webContents == null) return;

        if (mGestureStateListener != null) {
            GestureListenerManager.fromWebContents(webContents)
                    .removeListener(mGestureStateListener);
            mGestureStateListener = null;

            // If we needed to remove our listener, we also need to remove our selection client.
            if (mSelectionClientManager != null) {
                SelectionPopupController controller =
                        SelectionPopupController.fromWebContents(webContents);
                controller.setSelectionClient(
                        mSelectionClientManager.removeContextualSearchSelectionClient());
            }
            // Also make sure the UI is hidden if the device is offline.
            ContextualSearchManager contextualSearchManager = getContextualSearchManager(mTab);
            if (contextualSearchManager != null && !isDeviceOnline(contextualSearchManager)) {
                contextualSearchManager.hideContextualSearch(StateChangeReason.UNKNOWN);
            }
        }
    }

    /** @return whether Contextual Search is enabled and active in this tab. */
    private boolean isContextualSearchActive(WebContents webContents) {
        assert mTab.getWebContents() == null || mTab.getWebContents() == webContents;
        ContextualSearchManager manager = getContextualSearchManager(mTab);
        if (manager == null) return false;

        return !webContents.isIncognito() && FirstRunStatus.getFirstRunFlowComplete()
                && !ContextualSearchManager.isContextualSearchDisabled()
                && TemplateUrlServiceFactory.get().isDefaultSearchEngineGoogle()
                && !LocaleManager.getInstance().needToCheckForSearchEnginePromo()
                // Svelte and Accessibility devices are incompatible with the first-run flow and
                // Talkback has poor interaction with tap to search (see http://crbug.com/399708 and
                // http://crbug.com/396934).
                && !manager.isRunningInCompatibilityMode()
                && !(mTab.isShowingErrorPage() || mTab.isShowingInterstitialPage())
                && isDeviceOnline(manager) && mUnhookedTab == null;
    }

    /** @return Whether the device is online, or we have disabled online-detection. */
    private boolean isDeviceOnline(ContextualSearchManager manager) {
        return ContextualSearchFieldTrial.getSwitch(
                       ContextualSearchSwitch.IS_ONLINE_DETECTION_DISABLED)
                ? true
                : manager.isDeviceOnline();
    }

    /**
     * Gets the {@link ContextualSearchManager} associated with the given tab's activity.
     * @param tab The {@link Tab} that we're getting the manager for.
     * @return The Contextual Search manager controlling that Tab.
     */
    private ContextualSearchManager getContextualSearchManager(Tab tab) {
        Activity activity = tab.getWindowAndroid().getActivity().get();
        if (activity instanceof ChromeActivity) {
            return ((ChromeActivity) activity).getContextualSearchManager();
        }
        return null;
    }

    // ============================================================================================
    // Native support.
    // ============================================================================================

    @CalledByNative
    void onContextualSearchPrefChanged() {
        updateContextualSearchHooks(mWebContents);

        ContextualSearchManager manager = getContextualSearchManager(mTab);
        if (manager != null) {
            boolean isEnabled = !ContextualSearchManager.isContextualSearchDisabled()
                    && !ContextualSearchManager.isContextualSearchUninitialized();
            manager.onContextualSearchPrefChanged(isEnabled);
        }
    }

    /**
     * Notifies this helper to show the Unhandled Tap UI due to a tap at the given pixel
     * coordinates.
     */
    @CalledByNative
    void onShowUnhandledTapUIIfNeeded(int x, int y, int fontSizeDips, int textRunLength) {
        // Only notify the manager if we currently have a valid listener.
        if (mGestureStateListener != null && getContextualSearchManager(mTab) != null) {
            getContextualSearchManager(mTab).onShowUnhandledTapUIIfNeeded(
                    x, y, fontSizeDips, textRunLength);
        }
    }

    @NativeMethods
    interface Natives {
        long init(ContextualSearchTabHelper caller, Profile profile);
        void installUnhandledTapNotifierIfNeeded(long nativeContextualSearchTabHelper,
                ContextualSearchTabHelper caller, WebContents webContents, float pxToDpScaleFactor);
        void destroy(long nativeContextualSearchTabHelper, ContextualSearchTabHelper caller);
    }
}
