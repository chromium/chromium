// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.fullscreen;

import android.app.Activity;
import android.os.Handler;
import android.os.Message;
import android.view.View;
import android.view.View.OnLayoutChangeListener;
import android.view.Window;
import android.view.WindowManager;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;
import androidx.core.util.ObjectsCompat;

import org.chromium.base.ActivityState;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.ApplicationStatus.ActivityStateListener;
import org.chromium.base.ApplicationStatus.WindowFocusChangedListener;
import org.chromium.base.BuildInfo;
import org.chromium.base.ObserverList;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.cc.input.BrowserControlsState;
import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.ActivityTabProvider.ActivityTabTabObserver;
import org.chromium.chrome.browser.crash.ChromePureJavaExceptionReporter;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.multiwindow.MultiWindowUtils;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabAttributeKeys;
import org.chromium.chrome.browser.tab.TabAttributes;
import org.chromium.chrome.browser.tab.TabBrowserControlsConstraintsHelper;
import org.chromium.chrome.browser.tab.TabHidingType;
import org.chromium.chrome.browser.tab.TabUtils;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorTabObserver;
import org.chromium.chrome.browser.ui.edge_to_edge.EdgeToEdgeUtils;
import org.chromium.components.embedder_support.view.ContentView;
import org.chromium.content_public.browser.GestureListenerManager;
import org.chromium.content_public.browser.NavigationHandle;
import org.chromium.content_public.browser.SelectionPopupController;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.ViewUtils;

import java.lang.ref.WeakReference;
import java.util.Locale;

/** Handles updating the UI based on requests to the HTML Fullscreen API. */
public abstract class FullscreenHtmlApiHandlerBase
        implements ActivityStateListener, WindowFocusChangedListener, FullscreenManager {
    private static final boolean DEBUG_LOGS = false;

    protected static final int MSG_ID_SET_VISIBILITY_FOR_SYSTEM_BARS = 1;
    protected  static final int MSG_ID_UNSET_FULLSCREEN_LAYOUT = 2;

    // The time we allow the Android notification bar to be shown when it is requested temporarily
    // by the Android system (this value is additive on top of the show duration imposed by
    // Android).
    protected static final long ANDROID_CONTROLS_SHOW_DURATION_MS = 200;
    // Delay to allow a frame to render between getting the fullscreen layout update and leaving
    // layout fullscreen mode.
    private static final long CLEAR_LAYOUT_FULLSCREEN_DELAY_MS = 20;

    protected final Activity mActivity;
    protected final Handler mHandler;
    private final ObservableSupplierImpl<Boolean> mPersistentModeSupplier;
    private final ObservableSupplier<Boolean> mAreControlsHidden;
    private final boolean mExitFullscreenOnStop;
    private final ObserverList<FullscreenManager.Observer> mObservers = new ObserverList<>();

    // We need to cache WebContents/ContentView since we are setting fullscreen UI state on
    // the WebContents's container view, and a Tab can change to have null web contents/
    // content view, i.e., if you navigate to a native page.
    @Nullable private WebContents mWebContentsInFullscreen;
    @Nullable private View mContentViewInFullscreen;
    @Nullable protected Tab mTabInFullscreen;
    @Nullable private FullscreenOptions mFullscreenOptions;

    private FullscreenToast mToast;

    private OnLayoutChangeListener mFullscreenOnLayoutChangeListener;

    private FullscreenOptions mPendingFullscreenOptions;

    private ActivityTabTabObserver mActiveTabObserver;
    private TabModelSelectorTabObserver mTabFullscreenObserver;
    @Nullable private Tab mTab;

    private boolean mNotifyOnNextExit;

    // Current ContentView. Updates when active tab is switched or WebContents is swapped
    // in the current Tab.
    private ContentView mContentView;

    protected ContentView getContentView() {
        return mContentView;
    }

    /**
     * Update the current content view that can be shown in fullscreen mode, e.g. when the active
     * tab is switched or when web contents are swapped in the current Tab.
     * @param contentView The new content view.
     */
    protected void setContentView(ContentView contentView) {
        mContentView = contentView;
    }

    // This static inner class holds a WeakReference to the outer object, to avoid triggering the
    // lint HandlerLeak warning.
    private static class FullscreenHandler extends Handler {
        private final WeakReference<FullscreenHtmlApiHandlerBase> mFullscreenHtmlApiHandler;

        public FullscreenHandler(FullscreenHtmlApiHandlerBase fullscreenHtmlApiHandlerBase) {
            mFullscreenHtmlApiHandler =
                    new WeakReference<FullscreenHtmlApiHandlerBase>(fullscreenHtmlApiHandlerBase);
        }

        @Override
        public void handleMessage(Message msg) {
            if (msg == null) return;
            FullscreenHtmlApiHandlerBase fullscreenHtmlApiHandlerBase =
                    mFullscreenHtmlApiHandler.get();
            if (fullscreenHtmlApiHandlerBase == null) return;

            final WebContents webContents = fullscreenHtmlApiHandlerBase.mWebContentsInFullscreen;
            if (webContents == null) return;

            final View contentView = fullscreenHtmlApiHandlerBase.mContentViewInFullscreen;
            if (contentView == null) return;

            switch (msg.what) {
                case MSG_ID_SET_VISIBILITY_FOR_SYSTEM_BARS:
                    {
                        assert fullscreenHtmlApiHandlerBase.getPersistentFullscreenMode()
                                : "Calling after we exited fullscreen";
                        assert fullscreenHtmlApiHandlerBase.mFullscreenOptions != null;

                        if (!fullscreenHtmlApiHandlerBase.hasDesiredStateForSystemBars(
                                contentView, fullscreenHtmlApiHandlerBase.mFullscreenOptions)) {
                            fullscreenHtmlApiHandlerBase.hideSystemBars(
                                    contentView, fullscreenHtmlApiHandlerBase.mFullscreenOptions);
                            if (DEBUG_LOGS) {
                                fullscreenHtmlApiHandlerBase.logHandleMessageHideSystemBars(
                                        contentView);
                            }
                        }

                        if (!fullscreenHtmlApiHandlerBase.isLayoutFullscreen(contentView)) return;

                        // Trigger an update to unset layout fullscreen mode once the view has been
                        // laid out after this system UI update.  Without clearing this flag, the
                        // keyboard appearing will not trigger a relayout of the contents, which
                        // prevents updating the overdraw amount to the renderer.
                        contentView.addOnLayoutChangeListener(
                                new OnLayoutChangeListener() {
                                    @Override
                                    public void onLayoutChange(
                                            View v,
                                            int left,
                                            int top,
                                            int right,
                                            int bottom,
                                            int oldLeft,
                                            int oldTop,
                                            int oldRight,
                                            int oldBottom) {
                                        sendEmptyMessageDelayed(
                                                MSG_ID_UNSET_FULLSCREEN_LAYOUT,
                                                CLEAR_LAYOUT_FULLSCREEN_DELAY_MS);
                                        contentView.removeOnLayoutChangeListener(this);
                                    }
                                });

                        ViewUtils.requestLayout(
                                contentView,
                                "FullscreenHtmlApiHandler.FullscreenHandler.handleMessage");
                        break;
                    }
                case MSG_ID_UNSET_FULLSCREEN_LAYOUT:
                    {
                        // Change this assert to simply ignoring the message to work around
                        // https://crbug/365638
                        // TODO(aberent): Fix bug assert getPersistentFullscreenMode() : "Calling
                        // after we exited fullscreen";
                        if (!fullscreenHtmlApiHandlerBase.getPersistentFullscreenMode()) return;
                        fullscreenHtmlApiHandlerBase.unsetLayoutFullscreen(contentView);
                        if (DEBUG_LOGS) {
                            fullscreenHtmlApiHandlerBase.logHandlerUnsetFullscreenLayout(
                                    contentView);
                        }
                        fullscreenHtmlApiHandlerBase.unsetTranslucentStatusBar();
                        break;
                    }
                default:
                    assert false : "Unexpected message for ID: " + msg.what;
                    break;
            }
        }
    }

    /**
     * Constructs the handler that will manage the UI transitions from the HTML fullscreen API.
     *
     * @param activity The activity that supports fullscreen.
     * @param areControlsHidden Supplier of a flag indicating if browser controls are hidden.
     * @param exitFullscreenOnStop Whether fullscreen mode should exit on stop - should be true for
     *     Activities that are not always fullscreen.
     */
    public FullscreenHtmlApiHandlerBase(
            Activity activity,
            ObservableSupplier<Boolean> areControlsHidden,
            boolean exitFullscreenOnStop) {
        mActivity = activity;
        mAreControlsHidden = areControlsHidden;
        mAreControlsHidden.addObserver(this::maybeEnterFullscreenFromPendingState);
        mHandler = new FullscreenHandler(this);

        mPersistentModeSupplier = new ObservableSupplierImpl<>();
        mPersistentModeSupplier.set(false);
        mExitFullscreenOnStop = exitFullscreenOnStop;
    }

    /**
     * Initialize the FullscreeHtmlApiHandler.
     * @param activityTabProvider Provider of the current activity tab.
     * @param modelSelector The tab model selector that will be monitored for tab changes.
     */
    void initialize(ActivityTabProvider activityTabProvider, TabModelSelector modelSelector) {
        ApplicationStatus.registerStateListenerForActivity(this, mActivity);
        ApplicationStatus.registerWindowFocusChangedListener(this);
        mActiveTabObserver =
                new ActivityTabTabObserver(activityTabProvider) {
                    @Override
                    protected void onObservingDifferentTab(Tab tab, boolean hint) {
                        mTab = tab;
                        setContentView(tab != null ? tab.getContentView() : null);
                        if (tab != null) {
                            updateMultiTouchZoomSupport(!getPersistentFullscreenMode());
                        }
                    }
                };

        mTabFullscreenObserver =
                new TabModelSelectorTabObserver(modelSelector) {
                    @Override
                    public void onContentChanged(Tab tab) {
                        setContentView(tab.getContentView());
                    }

                    @Override
                    public void onHidden(Tab tab, @TabHidingType int reason) {
                        // Clean up any fullscreen state that might impact other tabs.
                        exitPersistentFullscreenMode();
                    }

                    @Override
                    public void onDidFinishNavigationInPrimaryMainFrame(
                            Tab tab, NavigationHandle navigation) {
                        if (!navigation.isSameDocument() && tab == modelSelector.getCurrentTab()) {
                            exitPersistentFullscreenMode();
                        }
                    }

                    @Override
                    public void onInteractabilityChanged(Tab tab, boolean interactable) {
                        // Compare |tab| with |TabModelSelector#getCurrentTab()| which is a safer
                        // indicator for the active tab than |mTab|, since the invocation order of
                        // ActivityTabTabObserver and TabModelSelectorTabObserver is not explicitly
                        // defined.
                        if (!interactable || tab != modelSelector.getCurrentTab()) return;
                        onTabInteractable(tab);
                    }
                };
    }

    @VisibleForTesting
    void onTabInteractable(Tab tab) {
        Runnable enterFullscreen = getAndClearEnterFullscreenRunnable(tab);
        if (enterFullscreen != null) enterFullscreen.run();
    }

    @Override
    public void addObserver(FullscreenManager.Observer observer) {
        mObservers.addObserver(observer);
    }

    @Override
    public void removeObserver(FullscreenManager.Observer observer) {
        mObservers.removeObserver(observer);
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    private FullscreenToast getToast() {
        if (mToast == null) {
            mToast = new FullscreenToast.AndroidToast(mActivity, this::getPersistentFullscreenMode);
        }
        return mToast;
    }

    @Override
    public void onEnterFullscreen(Tab tab, FullscreenOptions options) {
        if (shouldSkipEnterFullscreenRequest(options)) return;
        // If enabling fullscreen while the tab is not interactable, fullscreen
        // will be delayed until the tab is interactable.
        Runnable r =
                () -> {
                    enterPersistentFullscreenMode(options);
                    destroySelectActionMode(tab);
                    setEnterFullscreenRunnable(tab, null);
                    for (FullscreenManager.Observer observer : mObservers) {
                        observer.onEnterFullscreen(tab, options);
                    }
                };
        if (tab.isUserInteractable()) {
            r.run();
        } else {
            setEnterFullscreenRunnable(tab, r);
        }
    }

    private boolean shouldSkipEnterFullscreenRequest(FullscreenOptions options) {
        // Do not process the request again if we're already in fullscreen mode and the request
        // with the same option (could be in pending state) is received.
        return getPersistentFullscreenMode()
                && (ObjectsCompat.equals(mFullscreenOptions, options)
                        || ObjectsCompat.equals(mPendingFullscreenOptions, options));
    }

    @Override
    public void onExitFullscreen(Tab tab) {
        if (tab != mTab) return;
        setEnterFullscreenRunnable(tab, null);
        boolean wasInPersistentFullscreenMode = getPersistentFullscreenMode();
        exitPersistentFullscreenMode();
        if (wasInPersistentFullscreenMode || mNotifyOnNextExit) {
            mNotifyOnNextExit = false;
            for (FullscreenManager.Observer observer : mObservers) {
                observer.onExitFullscreen(tab);
            }
        }
    }

    /**
     * @see GestureListenerManager#updateMultiTouchZoomSupport(boolean).
     */
    @VisibleForTesting
    protected void updateMultiTouchZoomSupport(boolean enable) {
        if (mTab == null || mTab.isHidden()) return;
        WebContents webContents = mTab.getWebContents();
        if (webContents != null) {
            GestureListenerManager manager = GestureListenerManager.fromWebContents(webContents);
            if (manager != null) manager.updateMultiTouchZoomSupport(enable);
        }
    }

    @VisibleForTesting
    /* package */ void destroySelectActionMode(Tab tab) {
        WebContents webContents = tab.getWebContents();
        if (webContents != null) {
            SelectionPopupController.fromWebContents(webContents).destroySelectActionMode();
        }
    }

    private void setEnterFullscreenRunnable(Tab tab, Runnable runnable) {
        TabAttributes attrs = TabAttributes.from(tab);
        if (runnable == null) {
            attrs.clear(TabAttributeKeys.ENTER_FULLSCREEN);
        } else {
            attrs.set(TabAttributeKeys.ENTER_FULLSCREEN, runnable);
        }
    }

    private Runnable getAndClearEnterFullscreenRunnable(Tab tab) {
        Runnable r =
                tab != null ? TabAttributes.from(tab).get(TabAttributeKeys.ENTER_FULLSCREEN) : null;
        if (r != null) setEnterFullscreenRunnable(tab, null);
        return r;
    }

    /**
     * Enters persistent fullscreen mode. In this mode, the browser controls will be
     * permanently hidden until this mode is exited.
     *
     * @param options Options to choose mode of fullscreen.
     */
    private void enterPersistentFullscreenMode(FullscreenOptions options) {
        if (!shouldSkipEnterFullscreenRequest(options)) {
            mPersistentModeSupplier.set(true);
            mNotifyOnNextExit = true;
            if (mAreControlsHidden.get()) {
                // The browser controls are currently hidden.
                enterFullscreen(mTab, options);
            } else {
                // We should hide browser controls first.
                mPendingFullscreenOptions = options;
            }
        }
        updateMultiTouchZoomSupport(false);
    }

    /**
     * Enter fullscreen if there was a pending request due to browser controls yet to be hidden.
     * @param controlsHidden {@code true} if the controls are now hidden.
     */
    private void maybeEnterFullscreenFromPendingState(boolean controlsHidden) {
        if (!controlsHidden || mTab == null) return;
        if (mPendingFullscreenOptions != null) {
            if (mPendingFullscreenOptions.canceled()) {
                // Restore browser controls if the fullscreen process got canceled.
                TabBrowserControlsConstraintsHelper.update(mTab, BrowserControlsState.SHOWN, true);
            } else {
                enterFullscreen(mTab, mPendingFullscreenOptions);
            }
            mPendingFullscreenOptions = null;
        }
    }

    @Override
    public void exitPersistentFullscreenMode() {
        if (getPersistentFullscreenMode()) {
            getToast().onExitPersistentFullscreen();
            mPersistentModeSupplier.set(false);

            if (mWebContentsInFullscreen != null && mTabInFullscreen != null) {
                exitFullscreen(mWebContentsInFullscreen, mContentViewInFullscreen);
            } else {
                if (mPendingFullscreenOptions != null) mPendingFullscreenOptions.setCanceled();
                if (mAreControlsHidden.get()) {
                    TabBrowserControlsConstraintsHelper.update(
                            mTab, BrowserControlsState.SHOWN, true);
                }
            }
            mWebContentsInFullscreen = null;
            mContentViewInFullscreen = null;
            mTabInFullscreen = null;
            mFullscreenOptions = null;
        }
        updateMultiTouchZoomSupport(true);
    }

    @Override
    public boolean getPersistentFullscreenMode() {
        return mPersistentModeSupplier.get();
    }

    /**
     * @return An observable supplier that determines whether the app is in persistent fullscreen
     *     mode.
     */
    @Override
    public ObservableSupplier<Boolean> getPersistentFullscreenModeSupplier() {
        return mPersistentModeSupplier;
    }

    private void exitFullscreen(WebContents webContents, View contentView) {
        getToast().onExitFullscreen();
        mHandler.removeMessages(MSG_ID_SET_VISIBILITY_FOR_SYSTEM_BARS);
        mHandler.removeMessages(MSG_ID_UNSET_FULLSCREEN_LAYOUT);

        unsetTranslucentStatusBar();
        showSystemBars(contentView);
        if (DEBUG_LOGS) logExitFullscreen(contentView);
        resetExitFullscreenLayoutChangeListener(contentView);
        if (webContents != null && !webContents.isDestroyed()) webContents.exitFullscreen();

        // Ensure that the layout change listener to bring back browser controls is called on
        // automotive devices that never hide system bars.
        if (BuildInfo.getInstance().isAutomotive) {
            ViewUtils.requestLayout(contentView, "FullscreenHtmlApiHandler.exitFullScreen");
        }
    }

    private void resetExitFullscreenLayoutChangeListener(View contentView) {
        if (mFullscreenOnLayoutChangeListener != null) {
            contentView.removeOnLayoutChangeListener(mFullscreenOnLayoutChangeListener);
        }
        mFullscreenOnLayoutChangeListener =
                new OnLayoutChangeListener() {
                    @Override
                    public void onLayoutChange(
                            View v,
                            int left,
                            int top,
                            int right,
                            int bottom,
                            int oldLeft,
                            int oldTop,
                            int oldRight,
                            int oldBottom) {
                        showBrowserControlsOnFullscreenExit(
                                top, bottom, oldTop, oldBottom, contentView);
                    }
                };
        contentView.addOnLayoutChangeListener(mFullscreenOnLayoutChangeListener);
    }

    private void showBrowserControlsOnFullscreenExit(
            int top, int bottom, int oldTop, int oldBottom, View contentView) {
        boolean didLayoutGrow = (bottom - top) > (oldBottom - oldTop);
        // Only show the browser controls if the layout is shrinking (or staying the same). However,
        // this check should be bypassed on automotive.
        if (didLayoutGrow && !BuildInfo.getInstance().isAutomotive) {
            // If the dedicated flag is enabled, bypass this check and show the browser controls. A
            // report should also be logged to help confirm whether odd layout values are related
            // to multi-window mode / the edge-to-edge feature.
            if (ChromeFeatureList.sForceBrowserControlsUponExitingFullscreen.isEnabled()) {
                logBrowserControlsForcedUponFullscreenExit(top, bottom, oldTop, oldBottom);
            } else {
                return;
            }
        }

        // At this point, browser controls are hidden. Show browser controls only if it's permitted.
        TabBrowserControlsConstraintsHelper.update(mTab, BrowserControlsState.SHOWN, true);
        if (mFullscreenOnLayoutChangeListener != null) {
            contentView.removeOnLayoutChangeListener(mFullscreenOnLayoutChangeListener);
        }
    }

    private void logBrowserControlsForcedUponFullscreenExit(
            int top, int bottom, int oldTop, int oldBottom) {
        String message =
                String.format(
                        Locale.ENGLISH,
                        "This is not a crash. See"
                                + " https://crbug.com/363349568.\n"
                                + "top: %d, bottom: %d, oldTop: %d\n"
                                + "oldBottom: %d, inMultiWindowMode: %b, isEdgeToEdgeEnabled: %b",
                        top,
                        bottom,
                        oldTop,
                        oldBottom,
                        MultiWindowUtils.getInstance().isInMultiWindowMode(mActivity),
                        EdgeToEdgeUtils.isEnabled());
        ChromePureJavaExceptionReporter.reportJavaException(new Throwable(message));
    }

    private boolean isAlreadyInFullscreenOrNavigationHidden(View contentView) {
        return isStatusBarHidden(contentView) || isNavigationBarHidden(contentView);
    }

    private boolean hasDesiredStateForSystemBars(View contentView, FullscreenOptions options) {
        assert options != null;

        boolean shouldDisplayStatusBar = options.showStatusBar;
        boolean shouldDisplayNavigationBar = options.showNavigationBar;

        if (shouldDisplayStatusBar == isStatusBarHidden(contentView)) return false;
        if (shouldDisplayNavigationBar == isNavigationBarHidden(contentView)) return false;

        return true;
    }

    /**
     * Handles hiding the system UI components to allow the content to take up the full screen.
     *
     * @param tab The tab that is entering fullscreen.
     */
    private void enterFullscreen(final Tab tab, FullscreenOptions options) {
        assert !(options.showNavigationBar && options.showStatusBar)
                : "Cannot enter fullscreen with both status and navigation bars visible!";

        if (DEBUG_LOGS) logEnterFullscreenOptions(options);
        WebContents webContents = tab.getWebContents();
        if (webContents == null) return;
        mFullscreenOptions = options;
        final View contentView = tab.getContentView();

        if (isAlreadyInFullscreenOrNavigationHidden(contentView)) {
            // We are already in fullscreen mode and the fullscreen options match what is
            // needed; nothing to do.
            if (hasDesiredStateForSystemBars(contentView, mFullscreenOptions)) return;

            resetEnterFullscreenLayoutChangeListener(contentView);
            adjustSystemBarsInFullscreenMode(contentView, mFullscreenOptions);
        } else if (isLayoutFullscreen(contentView) || isLayoutHidingNavigation(contentView)) {
            resetEnterFullscreenLayoutChangeListener(contentView);
            hideSystemBars(contentView, mFullscreenOptions);
        } else {
            Activity activity = TabUtils.getActivity(tab);
            boolean isMultiWindow = MultiWindowUtils.getInstance().isInMultiWindowMode(activity);

            // To avoid a double layout that is caused by the system when just hiding
            // the status bar set the status bar as translucent immediately. This causes
            // it not to take up space so the layout is stable. (See https://crbug.com/935015).
            // Do not do this in multi-window mode or if the system bars can't be dismissed (i.e.
            // on some automotive devices), since the status bar will be forced to always stay
            // visible.
            if (!mFullscreenOptions.showStatusBar
                    && !isMultiWindow
                    && !BuildInfo.getInstance().isAutomotive) {
                setTranslucentStatusBar();
            }

            resetEnterFullscreenLayoutChangeListener(contentView);
            if (!mFullscreenOptions.showNavigationBar) hideNavigationBar(contentView);
            if (!mFullscreenOptions.showStatusBar) setLayoutFullscreen(contentView);
        }

        if (DEBUG_LOGS) logEnterFullscreen(contentView);

        // Request a layout so the updated system visibility takes affect.
        // The flow will continue in the handler of MSG_ID_UNSET_FULLSCREEN_LAYOUT message.
        ViewUtils.requestLayout(contentView, "FullscreenHtmlApiHandler.enterFullScreen");

        mWebContentsInFullscreen = webContents;
        mContentViewInFullscreen = contentView;
        mTabInFullscreen = tab;
        getToast().onEnterFullscreen();
    }

    private void resetEnterFullscreenLayoutChangeListener(View contentView) {
        if (mFullscreenOnLayoutChangeListener != null) {
            contentView.removeOnLayoutChangeListener(mFullscreenOnLayoutChangeListener);
        }

        mFullscreenOnLayoutChangeListener =
                new OnLayoutChangeListener() {
                    @Override
                    public void onLayoutChange(
                            View v,
                            int left,
                            int top,
                            int right,
                            int bottom,
                            int oldLeft,
                            int oldTop,
                            int oldRight,
                            int oldBottom) {
                        // On certain sites playing embedded video (http://crbug.com/293782),
                        // setting the layout as fullscreen does not always trigger a view-level
                        // layout with an updated height. To work around this, do not check for an
                        // increased height and always just trigger the next step of the
                        // fullscreen initialization.
                        // Posting the message to set the fullscreen flag because setting it
                        // directly in the onLayoutChange would have no effect.
                        mHandler.sendEmptyMessage(MSG_ID_SET_VISIBILITY_FOR_SYSTEM_BARS);

                        if ((bottom - top) <= (oldBottom - oldTop)
                                && (right - left) <= (oldRight - oldLeft)
                                // Some automotive devices never hide the system bars, so Chrome
                                // can't rely on detecting a change in insets.
                                && !BuildInfo.getInstance().isAutomotive) {
                            return;
                        }

                        getToast().onFullscreenLayout();
                        contentView.removeOnLayoutChangeListener(this);
                    }
                };

        contentView.addOnLayoutChangeListener(mFullscreenOnLayoutChangeListener);
    }

    // ActivityStateListener

    @Override
    public void onActivityStateChange(Activity activity, int newState) {
        if (newState == ActivityState.STOPPED && mExitFullscreenOnStop) {
            // Exit fullscreen in onStop to ensure the system UI flags are set correctly when
            // showing again (on JB MR2+ builds, the omnibox would be covered by the
            // notification bar when this was done in onStart()).
            exitPersistentFullscreenMode();
        } else if (newState == ActivityState.DESTROYED) {
            ApplicationStatus.unregisterActivityStateListener(this);
            ApplicationStatus.unregisterWindowFocusChangedListener(this);
        }
    }

    // WindowFocusChangedListener

    @Override
    public void onWindowFocusChanged(Activity activity, boolean hasWindowFocus) {
        if (mActivity != activity) return;

        // Window focus events can occur before the fullscreen toast is ready. It may skip and
        // wait till fullscreen is entered, by which time the toast object will be ready.
        if (mToast != null) mToast.onWindowFocusChanged(hasWindowFocus);

        mHandler.removeMessages(MSG_ID_SET_VISIBILITY_FOR_SYSTEM_BARS);
        mHandler.removeMessages(MSG_ID_UNSET_FULLSCREEN_LAYOUT);
        if (mTabInFullscreen == null || !getPersistentFullscreenMode() || !hasWindowFocus) return;
        mHandler.sendEmptyMessageDelayed(
                MSG_ID_SET_VISIBILITY_FOR_SYSTEM_BARS, ANDROID_CONTROLS_SHOW_DURATION_MS);
    }

    /*
     * Clears the current window attributes to not contain windowFlags. This
     * is slightly different that Window.clearFlags which then sets a
     * forced window attribute on the Window object that cannot be cleared.
     */
    protected void clearWindowFlags(int windowFlags) {
        Window window = mActivity.getWindow();
        final WindowManager.LayoutParams attrs = window.getAttributes();
        if ((attrs.flags & windowFlags) != 0) {
            attrs.flags &= ~windowFlags;
            window.setAttributes(attrs);
        }
    }

    /*
     * Sets the current window attributes to contain windowFlags. This
     * is slightly different that Window.setFlags which then sets a
     * forced window attribute on the Window object that cannot be cleared.
     */
    protected void setWindowFlags(int windowFlags) {
        Window window = mActivity.getWindow();
        final WindowManager.LayoutParams attrs = window.getAttributes();
        attrs.flags |= windowFlags;
        window.setAttributes(attrs);
    }

    /** Destroys the FullscreenHtmlApiHandler. */
    public void destroy() {
        mTab = null;
        setContentView(null);
        if (mActiveTabObserver != null) mActiveTabObserver.destroy();
        if (mTabFullscreenObserver != null) mTabFullscreenObserver.destroy();
        mObservers.clear();
    }

    void setTabForTesting(Tab tab) {
        mTab = tab;
    }

    ObserverList<FullscreenManager.Observer> getObserversForTesting() {
        return mObservers;
    }

    FullscreenOptions getPendingFullscreenOptionsForTesting() {
        return mPendingFullscreenOptions;
    }

    boolean isToastVisibleForTesting() {
        return getToast().isVisible();
    }

    /**
     * Hide the system bars (to enter fullscreen mode) based on the fullscreen options.
     * @param contentView The content view being shown or to be shown in fullscreen mode.
     * @param fullscreenOptions The fullscreen options to guide what UI is shown or hidden.
     */
    abstract void hideSystemBars(View contentView, FullscreenOptions fullscreenOptions);

    /**
     * Show the system bars (to exit fullscreen mode).
     * @param contentView The content view being shown or to be shown in fullscreen mode.
     */
    abstract void showSystemBars(View contentView);

    /**
     * Adjust the visibility of system bars while already in fullscreen mode.
     * @param contentView The content view being shown or to be shown in fullscreen mode.
     * @param fullscreenOptions The fullscreen options to guide what UI is shown or hidden.
     */
    abstract void adjustSystemBarsInFullscreenMode(
            View contentView, FullscreenOptions fullscreenOptions);

    /**
     * Whether the status bar is hidden.
     * @param contentView The content view being shown or to be shown in fullscreen mode.
     */
    abstract boolean isStatusBarHidden(View contentView);

    /**
     * Whether the navigation bar is hidden.
     * @param contentView The content view being shown or to be shown in fullscreen mode.
     */
    abstract boolean isNavigationBarHidden(View contentView);

    /**
     * Whether the view layout is laid out as if in fullscreen mode.
     * @param contentView The content view being shown or to be shown in fullscreen mode.
     */
    abstract boolean isLayoutFullscreen(View contentView);

    /**
     * Whether the view layout is laid out as if to hide the navigation bar.
     * @param contentView The content view being shown or to be shown in fullscreen mode.
     */
    abstract boolean isLayoutHidingNavigation(View contentView);

    /**
     * Hide the navigation bar to give more space to display the view.
     * @param contentView The content view being shown or to be shown in fullscreen mode.
     */
    abstract void hideNavigationBar(View contentView);

    /**
     * Request that the view's layout display itself in fullscreen mode.
     * @param contentView The content view being shown or to be shown in fullscreen mode.
     */
    abstract void setLayoutFullscreen(View contentView);

    /**
     * Remove the request to the view's layout to display itself in fullscreen mode.
     * @param contentView The content view being shown or to be shown in fullscreen mode.
     */
    abstract void unsetLayoutFullscreen(View contentView);

    /**
     * Set a translucent status for the status bar.
     */
    abstract void setTranslucentStatusBar();

    /**
     * Unset the status bar's translucent status.
     */
    abstract void unsetTranslucentStatusBar();

    /**
     * Logs when a view enters fullscreen mode.
     * @param contentView The content view being shown or to be shown in fullscreen mode.
     */
    abstract void logEnterFullscreen(View contentView);

    /**
     * Logs the fullscreen options when a view requests to enter fullscreen mode.
     * @param fullscreenOptions The content view being shown or to be shown in fullscreen mode.
     */
    abstract void logEnterFullscreenOptions(FullscreenOptions fullscreenOptions);

    /**
     * Logs when a view exits fullscreen mode.
     * @param contentView The content view being shown or to be shown in fullscreen mode.
     */
    abstract void logExitFullscreen(View contentView);

    /**
     * Logs when the handler processes a message to unset the view's layout being shown as if in
     * fullscreen mode.
     * @param contentView The content view being shown or to be shown in fullscreen mode.
     */
    abstract void logHandlerUnsetFullscreenLayout(View contentView);

    /**
     * Logs when the handler processes a message to hide the system bars.
     * @param contentView The content view being shown or to be shown in fullscreen mode.
     */
    abstract void logHandleMessageHideSystemBars(View contentView);
}
