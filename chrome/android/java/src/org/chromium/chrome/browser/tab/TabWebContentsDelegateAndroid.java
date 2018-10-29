// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab;

import android.annotation.TargetApi;
import android.app.Activity;
import android.app.ActivityManager;
import android.content.Context;
import android.content.Intent;
import android.graphics.Rect;
import android.graphics.RectF;
import android.media.AudioManager;
import android.os.Build;
import android.os.Handler;
import android.util.Pair;
import android.view.KeyEvent;
import android.view.View;

import org.chromium.base.ActivityState;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.BuildInfo;
import org.chromium.base.Log;
import org.chromium.base.ObserverList.RewindableIterator;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.blink_public.platform.WebDisplayMode;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.AppHooks;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.FullscreenActivity;
import org.chromium.chrome.browser.RepostFormWarningDialog;
import org.chromium.chrome.browser.SwipeRefreshHandler;
import org.chromium.chrome.browser.document.DocumentUtils;
import org.chromium.chrome.browser.document.DocumentWebContentsDelegate;
import org.chromium.chrome.browser.findinpage.FindMatchRectsDetails;
import org.chromium.chrome.browser.findinpage.FindNotificationDetails;
import org.chromium.chrome.browser.fullscreen.FullscreenOptions;
import org.chromium.chrome.browser.media.MediaCaptureNotificationService;
import org.chromium.chrome.browser.policy.PolicyAuditor;
import org.chromium.chrome.browser.policy.PolicyAuditor.AuditEvent;
import org.chromium.chrome.browser.tabmodel.TabCreatorManager.TabCreator;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModel.TabLaunchType;
import org.chromium.chrome.browser.tabmodel.TabModelUtils;
import org.chromium.chrome.browser.tabmodel.TabWindowManager;
import org.chromium.components.embedder_support.delegate.WebContentsDelegateAndroid;
import org.chromium.content_public.browser.GestureListenerManager;
import org.chromium.content_public.browser.InvalidateTypes;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.common.ResourceRequestBody;
import org.chromium.ui.mojom.WindowOpenDisposition;

/**
 * A basic {@link TabWebContentsDelegateAndroid} that forwards some calls to the registered
 * {@link TabObserver}s.
 */
public class TabWebContentsDelegateAndroid extends WebContentsDelegateAndroid {
    /**
     * Listener to be notified when a find result is received.
     */
    public interface FindResultListener {
        public void onFindResult(FindNotificationDetails result);
    }

    /**
     * Listener to be notified when the rects corresponding to find matches are received.
     */
    public interface FindMatchRectsListener {
        public void onFindMatchRects(FindMatchRectsDetails result);
    }

    /** Used for logging. */
    private static final String TAG = "WebContentsDelegate";

    protected final Tab mTab;

    private FindResultListener mFindResultListener;

    private FindMatchRectsListener mFindMatchRectsListener;

    private @WebDisplayMode int mDisplayMode = WebDisplayMode.BROWSER;

    protected Handler mHandler;

    private final Runnable mCloseContentsRunnable = new Runnable() {
        @Override
        public void run() {
            boolean isSelected = mTab.getTabModelSelector().getCurrentTab() == mTab;
            mTab.getTabModelSelector().closeTab(mTab);

            // If the parent Tab belongs to another Activity, fire the Intent to bring it back.
            if (isSelected && mTab.getParentIntent() != null
                    && mTab.getActivity().getIntent() != mTab.getParentIntent()) {
                mTab.getActivity().startActivity(mTab.getParentIntent());
            }
        }

        /** If the API allows it, returns whether a Task still exists for the parent Activity. */
        @TargetApi(Build.VERSION_CODES.LOLLIPOP)
        private boolean isParentInAndroidOverview() {
            ActivityManager activityManager = (ActivityManager) mTab.getApplicationContext()
                    .getSystemService(Context.ACTIVITY_SERVICE);
            for (ActivityManager.AppTask task : activityManager.getAppTasks()) {
                Intent taskIntent = DocumentUtils.getBaseIntentFromTask(task);
                if (taskIntent != null && taskIntent.filterEquals(mTab.getParentIntent())) {
                    return true;
                }
            }
            return false;
        }
    };

    public TabWebContentsDelegateAndroid(Tab tab) {
        mTab = tab;
        mHandler = new Handler();
    }

    /**
     * Sets the current display mode which can be queried using media queries.
     */
    public void setDisplayMode(@WebDisplayMode int displayMode) {
        mDisplayMode = displayMode;
    }

    @CalledByNative
    private @WebDisplayMode int getDisplayMode() {
        return mDisplayMode;
    }

    @CalledByNative
    private void onFindResultAvailable(FindNotificationDetails result) {
        if (mFindResultListener != null) {
            mFindResultListener.onFindResult(result);
        }
    }

    @Override
    public boolean addMessageToConsole(int level, String message, int lineNumber, String sourceId) {
        // Only output console.log messages on debug variants of Android OS. crbug/869804
        return !BuildInfo.isDebugAndroid();
    }

    @CalledByNative
    private void onFindMatchRectsAvailable(FindMatchRectsDetails result) {
        if (mFindMatchRectsListener != null) {
            mFindMatchRectsListener.onFindMatchRects(result);
        }
    }

    /** Register to receive the results of startFinding calls. */
    public void setFindResultListener(FindResultListener listener) {
        mFindResultListener = listener;
    }

    /** Register to receive the results of requestFindMatchRects calls. */
    public void setFindMatchRectsListener(FindMatchRectsListener listener) {
        mFindMatchRectsListener = listener;
    }

    // Helper functions used to create types that are part of the public interface
    @CalledByNative
    private static Rect createRect(int x, int y, int right, int bottom) {
        return new Rect(x, y, right, bottom);
    }

    @CalledByNative
    private static RectF createRectF(float x, float y, float right, float bottom) {
        return new RectF(x, y, right, bottom);
    }

    @CalledByNative
    private static FindNotificationDetails createFindNotificationDetails(
            int numberOfMatches, Rect rendererSelectionRect,
            int activeMatchOrdinal, boolean finalUpdate) {
        return new FindNotificationDetails(numberOfMatches, rendererSelectionRect,
                activeMatchOrdinal, finalUpdate);
    }

    @CalledByNative
    private static FindMatchRectsDetails createFindMatchRectsDetails(
            int version, int numRects, RectF activeRect) {
        return new FindMatchRectsDetails(version, numRects, activeRect);
    }

    @CalledByNative
    private static void setMatchRectByIndex(
            FindMatchRectsDetails findMatchRectsDetails, int index, RectF rect) {
        findMatchRectsDetails.rects[index] = rect;
    }

    @Override
    public void onLoadProgressChanged(int progress) {
        if (!mTab.isLoading()) return;
        mTab.notifyLoadProgress(mTab.getProgress());
    }

    @Override
    public void loadingStateChanged(boolean toDifferentDocument) {
        boolean isLoading = mTab.getWebContents() != null && mTab.getWebContents().isLoading();
        if (isLoading) {
            mTab.onLoadStarted(toDifferentDocument);
        } else {
            mTab.onLoadStopped();
        }
    }

    @Override
    public void onUpdateUrl(String url) {
        RewindableIterator<TabObserver> observers = mTab.getTabObservers();
        while (observers.hasNext()) {
            observers.next().onUpdateUrl(mTab, url);
        }
    }

    @Override
    public void showRepostFormWarningDialog() {
        // When the dialog is visible, keeping the refresh animation active
        // in the background is distracting and unnecessary (and likely to
        // jank when the dialog is shown).
        SwipeRefreshHandler handler = SwipeRefreshHandler.get(mTab);
        if (handler != null) handler.reset();

        if (mTab.getActivity() == null) return;
        RepostFormWarningDialog warningDialog = new RepostFormWarningDialog(mTab);
        warningDialog.show(mTab.getActivity().getFragmentManager(), null);
    }

    @Override
    public void enterFullscreenModeForTab(boolean prefersNavigationBar) {
        FullscreenOptions options = new FullscreenOptions(prefersNavigationBar);
        if (FullscreenActivity.shouldUseFullscreenActivity(mTab)) {
            FullscreenActivity.enterFullscreenMode(mTab, options);
        } else {
            mTab.enterFullscreenMode(options);
        }
    }

    @Override
    public void exitFullscreenModeForTab() {
        if (FullscreenActivity.shouldUseFullscreenActivity(mTab)) {
            FullscreenActivity.exitFullscreenMode(mTab);
        } else {
            mTab.exitFullscreenMode();
        }
    }

    @Override
    public void navigationStateChanged(int flags) {
        if ((flags & InvalidateTypes.TAB) != 0) {
            int mediaType = MediaCaptureNotificationService.getMediaType(
                    isCapturingAudio(), isCapturingVideo(), isCapturingScreen());
            MediaCaptureNotificationService.updateMediaNotificationForTab(
                    mTab.getApplicationContext(), mTab.getId(), mediaType, mTab.getUrl());
        }
        if ((flags & InvalidateTypes.TITLE) != 0) {
            // Update cached title then notify observers.
            mTab.updateTitle();
        }
        if ((flags & InvalidateTypes.URL) != 0) {
            RewindableIterator<TabObserver> observers = mTab.getTabObservers();
            while (observers.hasNext()) {
                observers.next().onUrlUpdated(mTab);
            }
        }
    }

    @Override
    public void visibleSSLStateChanged() {
        RewindableIterator<TabObserver> observers = mTab.getTabObservers();
        while (observers.hasNext()) {
            observers.next().onSSLStateUpdated(mTab);
        }
    }

    @Override
    public void webContentsCreated(WebContents sourceWebContents, long openerRenderProcessId,
            long openerRenderFrameId, String frameName, String targetUrl,
            WebContents newWebContents) {
        RewindableIterator<TabObserver> observers = mTab.getTabObservers();
        while (observers.hasNext()) {
            observers.next().webContentsCreated(mTab, sourceWebContents, openerRenderProcessId,
                    openerRenderFrameId, frameName, targetUrl, newWebContents);
        }
        // The URL can't be taken from the WebContents if it's paused.  Save it for later.
        assert mWebContentsUrlMapping == null;
        mWebContentsUrlMapping = Pair.create(newWebContents, targetUrl);

        // TODO(dfalcantara): Re-remove this once crbug.com/508366 is fixed.
        TabCreator tabCreator = mTab.getActivity().getTabCreator(mTab.isIncognito());

        if (tabCreator != null && tabCreator.createsTabsAsynchronously()) {
            DocumentWebContentsDelegate.getInstance().attachDelegate(newWebContents);
        }
    }

    @Override
    public void rendererUnresponsive() {
        super.rendererUnresponsive();
        if (mTab.getWebContents() != null) nativeOnRendererUnresponsive(mTab.getWebContents());
        mTab.handleRendererResponsiveStateChanged(false);
    }

    @Override
    public void rendererResponsive() {
        super.rendererResponsive();
        if (mTab.getWebContents() != null) nativeOnRendererResponsive(mTab.getWebContents());
        mTab.handleRendererResponsiveStateChanged(true);
    }

    @Override
    public boolean isFullscreenForTabOrPending() {
        return mTab.getFullscreenManager() == null
                ? false : mTab.getFullscreenManager().getPersistentFullscreenMode();
    }

    @Override
    public void openNewTab(String url, String extraHeaders, ResourceRequestBody postData,
            int disposition, boolean isRendererInitiated) {
        mTab.openNewTab(url, extraHeaders, postData, disposition, true, isRendererInitiated);
    }

    private Pair<WebContents, String> mWebContentsUrlMapping;

    protected TabModel getTabModel() {
        // TODO(dfalcantara): Remove this when DocumentActivity.getTabModelSelector()
        //                    can return a TabModelSelector that activateContents() can use.
        return mTab.getTabModelSelector().getModel(mTab.isIncognito());
    }

    @CalledByNative
    public boolean shouldResumeRequestsForCreatedWindow() {
        // Pause the WebContents if an Activity has to be created for it first.
        TabCreator tabCreator = mTab.getActivity().getTabCreator(mTab.isIncognito());
        assert tabCreator != null;
        return !tabCreator.createsTabsAsynchronously();
    }

    @CalledByNative
    public boolean addNewContents(WebContents sourceWebContents, WebContents webContents,
            int disposition, Rect initialPosition, boolean userGesture) {
        assert mWebContentsUrlMapping.first == webContents;

        TabCreator tabCreator = mTab.getActivity().getTabCreator(mTab.isIncognito());
        assert tabCreator != null;

        // Grab the URL, which might not be available via the Tab.
        String url = mWebContentsUrlMapping.second;
        mWebContentsUrlMapping = null;

        // Skip opening a new Tab if it doesn't make sense.
        if (mTab.isClosing()) return false;

        // Creating new Tabs asynchronously requires starting a new Activity to create the Tab,
        // so the Tab returned will always be null.  There's no way to know synchronously
        // whether the Tab is created, so assume it's always successful.
        boolean createdSuccessfully = tabCreator.createTabWithWebContents(mTab,
                webContents, mTab.getId(), TabLaunchType.FROM_LONGPRESS_FOREGROUND, url);
        boolean success = tabCreator.createsTabsAsynchronously() || createdSuccessfully;
        if (success && disposition == WindowOpenDisposition.NEW_POPUP) {
            PolicyAuditor auditor = AppHooks.get().getPolicyAuditor();
            auditor.notifyAuditEvent(mTab.getApplicationContext(),
                    AuditEvent.OPEN_POPUP_URL_SUCCESS, url, "");
        }

        return success;
    }

    @Override
    public void activateContents() {
        ChromeActivity activity = mTab.getActivity();
        if (activity == null) {
            Log.e(TAG, "Activity not set activateContents().  Bailing out.");
            return;
        }
        if (activity.isActivityDestroyed()) {
            Log.e(TAG, "Activity destroyed before calling activateContents().  Bailing out.");
            return;
        }
        if (!mTab.isInitialized()) {
            Log.e(TAG, "Tab not initialized before calling activateContents().  Bailing out.");
            return;
        }

        // Do nothing if the tab can currently be interacted with by the user.
        if (mTab.isUserInteractable()) return;

        TabModel model = getTabModel();
        int index = model.indexOf(mTab);
        if (index == TabModel.INVALID_TAB_INDEX) return;
        TabModelUtils.setIndex(model, index);

        // Do nothing if the activity is visible (STOPPED is the only valid invisible state as we
        // explicitly check isActivityDestroyed above).
        if (ApplicationStatus.getStateForActivity(activity) == ActivityState.STOPPED) {
            bringActivityToForeground();
        }
    }

    /**
     * Brings chrome's Activity to foreground, if it is not so.
     */
    protected void bringActivityToForeground() {
        // This intent is sent in order to get the activity back to the foreground if it was
        // not already. The previous call will activate the right tab in the context of the
        // TabModel but will only show the tab to the user if Chrome was already in the
        // foreground.
        // The intent is getting the tabId mostly because it does not cost much to do so.
        // When receiving the intent, the tab associated with the tabId should already be
        // active.
        // Note that calling only the intent in order to activate the tab is slightly slower
        // because it will change the tab when the intent is handled, which happens after
        // Chrome gets back to the foreground.
        Intent newIntent = Tab.createBringTabToFrontIntent(mTab.getId());
        if (newIntent != null) {
            newIntent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
            mTab.getApplicationContext().startActivity(newIntent);
        }
    }

    @Override
    public void closeContents() {
        // Execute outside of callback, otherwise we end up deleting the native
        // objects in the middle of executing methods on them.
        mHandler.removeCallbacks(mCloseContentsRunnable);
        mHandler.post(mCloseContentsRunnable);
    }

    @Override
    public boolean takeFocus(boolean reverse) {
        Activity activity = mTab.getActivity();
        if (activity == null) return false;
        if (reverse) {
            View menuButton = activity.findViewById(R.id.menu_button);
            if (menuButton != null && menuButton.isShown()) {
                return menuButton.requestFocus();
            }

            View tabSwitcherButton = activity.findViewById(R.id.tab_switcher_button);
            if (tabSwitcherButton != null && tabSwitcherButton.isShown()) {
                return tabSwitcherButton.requestFocus();
            }
        } else {
            View urlBar = activity.findViewById(R.id.url_bar);
            if (urlBar != null) return urlBar.requestFocus();
        }
        return false;
    }

    @Override
    public void handleKeyboardEvent(KeyEvent event) {
        if (event.getAction() == KeyEvent.ACTION_DOWN && mTab.getActivity() != null) {
            if (mTab.getActivity().onKeyDown(event.getKeyCode(), event)) return;

            // Handle the Escape key here (instead of in KeyboardShortcuts.java), so it doesn't
            // interfere with other parts of the activity (e.g. the URL bar).
            if (event.getKeyCode() == KeyEvent.KEYCODE_ESCAPE && event.hasNoModifiers()) {
                WebContents wc = mTab.getWebContents();
                if (wc != null) wc.stop();
                return;
            }
        }
        handleMediaKey(event);
    }

    /**
     * Redispatches unhandled media keys. This allows bluetooth headphones with play/pause or
     * other buttons to function correctly.
     */
    @TargetApi(19)
    private void handleMediaKey(KeyEvent e) {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.KITKAT) return;
        switch (e.getKeyCode()) {
            case KeyEvent.KEYCODE_MUTE:
            case KeyEvent.KEYCODE_HEADSETHOOK:
            case KeyEvent.KEYCODE_MEDIA_PLAY:
            case KeyEvent.KEYCODE_MEDIA_PAUSE:
            case KeyEvent.KEYCODE_MEDIA_PLAY_PAUSE:
            case KeyEvent.KEYCODE_MEDIA_STOP:
            case KeyEvent.KEYCODE_MEDIA_NEXT:
            case KeyEvent.KEYCODE_MEDIA_PREVIOUS:
            case KeyEvent.KEYCODE_MEDIA_REWIND:
            case KeyEvent.KEYCODE_MEDIA_RECORD:
            case KeyEvent.KEYCODE_MEDIA_FAST_FORWARD:
            case KeyEvent.KEYCODE_MEDIA_CLOSE:
            case KeyEvent.KEYCODE_MEDIA_EJECT:
            case KeyEvent.KEYCODE_MEDIA_AUDIO_TRACK:
                AudioManager am = (AudioManager) mTab.getApplicationContext().getSystemService(
                        Context.AUDIO_SERVICE);
                am.dispatchMediaKeyEvent(e);
                break;
            default:
                break;
        }
    }

    /**
     * @return Whether audio is being captured.
     */
    private boolean isCapturingAudio() {
        return !mTab.isClosing() && nativeIsCapturingAudio(mTab.getWebContents());
    }

    /**
     * @return Whether video is being captured.
     */
    private boolean isCapturingVideo() {
        return !mTab.isClosing() && nativeIsCapturingVideo(mTab.getWebContents());
    }

    /**
     * @return Whether screen is being captured.
     */
    private boolean isCapturingScreen() {
        return !mTab.isClosing() && nativeIsCapturingScreen(mTab.getWebContents());
    }

    /**
     * When STOP button in the media capture notification is clicked, pass the event to native
     * to stop the media capture.
     */
    public static void notifyStopped(int tabId) {
        final Tab tab = TabWindowManager.getInstance().getTabById(tabId);
        if (tab != null) nativeNotifyStopped(tab.getWebContents());
    }

    @CalledByNative
    private void setOverlayMode(boolean useOverlayMode) {
        mTab.getActivity().setOverlayMode(useOverlayMode);
    }

    @Override
    public int getTopControlsHeight() {
        return mTab.getTopControlsHeight();
    }

    @Override
    public int getBottomControlsHeight() {
        return mTab.getBottomControlsHeight();
    }

    @Override
    public boolean controlsResizeView() {
        return mTab.controlsResizeView();
    }

    /**
     *  This is currently called when committing a pre-rendered page or activating a portal.
     */
    @CalledByNative
    private void swapWebContents(
            WebContents webContents, boolean didStartLoad, boolean didFinishLoad) {
        mTab.swapWebContents(webContents, didStartLoad, didFinishLoad);
    }

    private float getDipScale() {
        return mTab.getWindowAndroid().getDisplay().getDipScale();
    }

    private void enableDoubleTap(boolean enable) {
        WebContents wc = mTab.getWebContents();
        GestureListenerManager gestureManager =
                wc != null ? GestureListenerManager.fromWebContents(wc) : null;
        if (gestureManager != null) gestureManager.updateDoubleTapSupport(enable);
    }

    public void showFramebustBlockInfobarForTesting(String url) {
        nativeShowFramebustBlockInfoBar(mTab.getWebContents(), url);
    }

    private static native void nativeOnRendererUnresponsive(WebContents webContents);
    private static native void nativeOnRendererResponsive(WebContents webContents);
    private static native boolean nativeIsCapturingAudio(WebContents webContents);
    private static native boolean nativeIsCapturingVideo(WebContents webContents);
    private static native boolean nativeIsCapturingScreen(WebContents webContents);
    private static native void nativeNotifyStopped(WebContents webContents);
    private static native void nativeShowFramebustBlockInfoBar(WebContents webContents, String url);
}
