// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.fullscreen;

import static android.view.View.SYSTEM_UI_FLAG_FULLSCREEN;
import static android.view.View.SYSTEM_UI_FLAG_HIDE_NAVIGATION;
import static android.view.View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY;
import static android.view.View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN;
import static android.view.View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION;
import static android.view.View.SYSTEM_UI_FLAG_LOW_PROFILE;

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
import org.chromium.base.Log;
import org.chromium.base.ObserverList;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.cc.input.BrowserControlsState;
import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.ActivityTabProvider.ActivityTabTabObserver;
import org.chromium.chrome.browser.multiwindow.MultiWindowUtils;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabAttributeKeys;
import org.chromium.chrome.browser.tab.TabAttributes;
import org.chromium.chrome.browser.tab.TabBrowserControlsConstraintsHelper;
import org.chromium.chrome.browser.tab.TabHidingType;
import org.chromium.chrome.browser.tab.TabUtils;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorTabObserver;
import org.chromium.components.embedder_support.view.ContentView;
import org.chromium.content_public.browser.GestureListenerManager;
import org.chromium.content_public.browser.NavigationHandle;
import org.chromium.content_public.browser.SelectionPopupController;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.ViewUtils;

import java.lang.ref.WeakReference;

/**
 * Handles updating the UI based on requests to the HTML Fullscreen API.
 */
public class FullscreenHtmlApiHandler implements ActivityStateListener, WindowFocusChangedListener,
                                                 View.OnSystemUiVisibilityChangeListener,
                                                 FullscreenManager {
    // TAG length is limited to 20 characters, so we cannot use full class name:
    private static final String TAG = "FullscreenHtmlApi";
    private static final boolean DEBUG_LOGS = false;

    private static final int MSG_ID_SET_FULLSCREEN_SYSTEM_UI_FLAGS = 1;
    private static final int MSG_ID_CLEAR_LAYOUT_FULLSCREEN_FLAG = 2;

    // The time we allow the Android notification bar to be shown when it is requested temporarily
    // by the Android system (this value is additive on top of the show duration imposed by
    // Android).
    private static final long ANDROID_CONTROLS_SHOW_DURATION_MS = 200;
    // Delay to allow a frame to render between getting the fullscreen layout update and clearing
    // the SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN flag.
    private static final long CLEAR_LAYOUT_FULLSCREEN_DELAY_MS = 20;

    private final Activity mActivity;
    private final Handler mHandler;
    private final ObservableSupplierImpl<Boolean> mPersistentModeSupplier;
    private final ObservableSupplier<Boolean> mAreControlsHidden;
    private final boolean mExitFullscreenOnStop;
    private final ObserverList<FullscreenManager.Observer> mObservers = new ObserverList<>();

    // We need to cache WebContents/ContentView since we are setting fullscreen UI state on
    // the WebContents's container view, and a Tab can change to have null web contents/
    // content view, i.e., if you navigate to a native page.
    @Nullable
    private WebContents mWebContentsInFullscreen;
    @Nullable
    private View mContentViewInFullscreen;
    @Nullable
    private Tab mTabInFullscreen;
    @Nullable
    private FullscreenOptions mFullscreenOptions;

    private FullscreenToast mToast;

    private OnLayoutChangeListener mFullscreenOnLayoutChangeListener;

    private FullscreenOptions mPendingFullscreenOptions;

    private ActivityTabTabObserver mActiveTabObserver;
    private TabModelSelectorTabObserver mTabFullscreenObserver;
    @Nullable
    private Tab mTab;

    private boolean mNotifyOnNextExit;

    // Current ContentView. Updates when active tab is switched or WebContents is swapped
    // in the current Tab.
    private ContentView mContentView;

    // This static inner class holds a WeakReference to the outer object, to avoid triggering the
    // lint HandlerLeak warning.
    private static class FullscreenHandler extends Handler {
        private final WeakReference<FullscreenHtmlApiHandler> mFullscreenHtmlApiHandler;

        public FullscreenHandler(FullscreenHtmlApiHandler fullscreenHtmlApiHandler) {
            mFullscreenHtmlApiHandler =
                    new WeakReference<FullscreenHtmlApiHandler>(fullscreenHtmlApiHandler);
        }

        @Override
        public void handleMessage(Message msg) {
            if (msg == null) return;
            FullscreenHtmlApiHandler fullscreenHtmlApiHandler = mFullscreenHtmlApiHandler.get();
            if (fullscreenHtmlApiHandler == null) return;

            final WebContents webContents = fullscreenHtmlApiHandler.mWebContentsInFullscreen;
            if (webContents == null) return;

            final View contentView = fullscreenHtmlApiHandler.mContentViewInFullscreen;
            if (contentView == null) return;
            int systemUiVisibility = contentView.getSystemUiVisibility();

            switch (msg.what) {
                case MSG_ID_SET_FULLSCREEN_SYSTEM_UI_FLAGS: {
                    assert fullscreenHtmlApiHandler.getPersistentFullscreenMode() :
                        "Calling after we exited fullscreen";

                    if (!hasDesiredStatusBarAndNavigationState(
                                systemUiVisibility, fullscreenHtmlApiHandler.mFullscreenOptions)) {
                        systemUiVisibility = fullscreenHtmlApiHandler.applyEnterFullscreenUIFlags(
                                systemUiVisibility);

                        if (DEBUG_LOGS) {
                            Log.i(TAG,
                                    "handleMessage set flags, systemUiVisibility="
                                            + systemUiVisibility);
                        }
                        setSystemUiVisibility(contentView, systemUiVisibility);
                    }

                    if ((systemUiVisibility & SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN) == 0) {
                        return;
                    }

                    // Trigger a update to clear the SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN flag
                    // once the view has been laid out after this system UI update.  Without
                    // clearing this flag, the keyboard appearing will not trigger a relayout
                    // of the contents, which prevents updating the overdraw amount to the
                    // renderer.
                    contentView.addOnLayoutChangeListener(new OnLayoutChangeListener() {
                        @Override
                        public void onLayoutChange(View v, int left, int top, int right, int bottom,
                                int oldLeft, int oldTop, int oldRight, int oldBottom) {
                            sendEmptyMessageDelayed(MSG_ID_CLEAR_LAYOUT_FULLSCREEN_FLAG,
                                    CLEAR_LAYOUT_FULLSCREEN_DELAY_MS);
                            contentView.removeOnLayoutChangeListener(this);
                        }
                    });

                    ViewUtils.requestLayout(contentView,
                            "FullscreenHtmlApiHandler.FullscreenHandler.handleMessage");
                    break;
                }
                case MSG_ID_CLEAR_LAYOUT_FULLSCREEN_FLAG: {
                    // Change this assert to simply ignoring the message to work around
                    // https://crbug/365638
                    // TODO(aberent): Fix bug
                    // assert getPersistentFullscreenMode() : "Calling after we exited fullscreen";
                    if (!fullscreenHtmlApiHandler.getPersistentFullscreenMode()) return;

                    systemUiVisibility &= ~SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN;
                    if (DEBUG_LOGS) {
                        Log.i(TAG,
                                "handleMessage clear fullscreen flag, systemUiVisibility="
                                        + systemUiVisibility);
                    }
                    setSystemUiVisibility(contentView, systemUiVisibility);
                    fullscreenHtmlApiHandler.clearWindowFlags(
                            WindowManager.LayoutParams.FLAG_TRANSLUCENT_STATUS);
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
     * @param exitFullscreenOnStop Whether fullscreen mode should exit on stop - should be
     *                             true for Activities that are not always fullscreen.
     */
    public FullscreenHtmlApiHandler(Activity activity,
            ObservableSupplier<Boolean> areControlsHidden, boolean exitFullscreenOnStop) {
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
        mActiveTabObserver = new ActivityTabTabObserver(activityTabProvider) {
            @Override
            protected void onObservingDifferentTab(Tab tab, boolean hint) {
                mTab = tab;
                setContentView(tab != null ? tab.getContentView() : null);
                if (tab != null) updateMultiTouchZoomSupport(!getPersistentFullscreenMode());
            }
        };

        mTabFullscreenObserver = new TabModelSelectorTabObserver(modelSelector) {
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
                if (!navigation.isSameDocument()) {
                    if (tab == modelSelector.getCurrentTab()) exitPersistentFullscreenMode();
                }
            }

            @Override
            public void onInteractabilityChanged(Tab tab, boolean interactable) {
                // Compare |tab| with |TabModelSelector#getCurrentTab()| which is a safer
                // indicator for the active tab than |mTab|, since the invocation order of
                // ActivityTabTabObserver and TabModelSelectorTabObserver is not explicitly defined.
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

    private void setContentView(ContentView contentView) {
        if (contentView == mContentView) return;
        if (mContentView != null) {
            mContentView.removeOnSystemUiVisibilityChangeListener(this);
        }
        mContentView = contentView;
        if (mContentView != null) {
            mContentView.addOnSystemUiVisibilityChangeListener(this);
        }
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
        Runnable r = () -> {
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
    private void updateMultiTouchZoomSupport(boolean enable) {
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
                exitFullscreen(
                        mWebContentsInFullscreen, mContentViewInFullscreen, mTabInFullscreen);
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
     *         mode.
     */
    @Override
    public ObservableSupplier<Boolean> getPersistentFullscreenModeSupplier() {
        return mPersistentModeSupplier;
    }

    private void exitFullscreen(WebContents webContents, View contentView, Tab tab) {
        getToast().onExitFullscreen();
        mHandler.removeMessages(MSG_ID_SET_FULLSCREEN_SYSTEM_UI_FLAGS);
        mHandler.removeMessages(MSG_ID_CLEAR_LAYOUT_FULLSCREEN_FLAG);

        int systemUiVisibility = contentView.getSystemUiVisibility();
        systemUiVisibility = applyExitFullscreenUIFlags(systemUiVisibility);
        clearWindowFlags(WindowManager.LayoutParams.FLAG_TRANSLUCENT_STATUS);
        if (DEBUG_LOGS) Log.i(TAG, "exitFullscreen, systemUiVisibility=" + systemUiVisibility);
        setSystemUiVisibility(contentView, systemUiVisibility);
        if (mFullscreenOnLayoutChangeListener != null) {
            contentView.removeOnLayoutChangeListener(mFullscreenOnLayoutChangeListener);
        }
        // Since automotive devices persist the system bars in full screen mode, onLayoutChange is
        // not triggered by showing the system bars, and browser controls need to be re-added
        // directly.
        if (BuildInfo.getInstance().isAutomotive) {
            TabBrowserControlsConstraintsHelper.update(mTab, BrowserControlsState.SHOWN, true);
        } else {
            mFullscreenOnLayoutChangeListener = new OnLayoutChangeListener() {
                @Override
                public void onLayoutChange(View v, int left, int top, int right, int bottom,
                        int oldLeft, int oldTop, int oldRight, int oldBottom) {
                    if ((bottom - top) <= (oldBottom - oldTop)) {
                        // At this point, browser controls are hidden. Show browser controls only if
                        // it's permitted.
                        TabBrowserControlsConstraintsHelper.update(
                                mTab, BrowserControlsState.SHOWN, true);
                        contentView.removeOnLayoutChangeListener(this);
                    }
                }
            };
            contentView.addOnLayoutChangeListener(mFullscreenOnLayoutChangeListener);
        }

        if (webContents != null && !webContents.isDestroyed()) webContents.exitFullscreen();
    }

    private static boolean isAlreadyInFullscreenOrNavigationHidden(int systemUiVisibility) {
        return (systemUiVisibility & SYSTEM_UI_FLAG_FULLSCREEN) == SYSTEM_UI_FLAG_FULLSCREEN
                || (systemUiVisibility & SYSTEM_UI_FLAG_HIDE_NAVIGATION)
                == SYSTEM_UI_FLAG_HIDE_NAVIGATION;
    }

    private static boolean hasDesiredStatusBarAndNavigationState(
            int systemUiVisibility, FullscreenOptions options) {
        assert options != null;

        boolean shouldDisplayStatusBar = options.showStatusBar;
        boolean shouldDisplayNavigationBar = options.showNavigationBar;

        boolean statusBarVisible =
                (systemUiVisibility & SYSTEM_UI_FLAG_FULLSCREEN) != SYSTEM_UI_FLAG_FULLSCREEN;
        boolean navigationBarVisible = (systemUiVisibility & SYSTEM_UI_FLAG_HIDE_NAVIGATION)
                != SYSTEM_UI_FLAG_HIDE_NAVIGATION;

        if (statusBarVisible != shouldDisplayStatusBar) return false;
        if (navigationBarVisible != shouldDisplayNavigationBar) return false;

        return true;
    }

    /**
     * Handles hiding the system UI components to allow the content to take up the full screen.
     * @param tab The tab that is entering fullscreen.
     */
    public void enterFullscreen(final Tab tab, FullscreenOptions options) {
        assert !(options.showNavigationBar && options.showStatusBar)
            : "Cannot enter fullscreen with both status and navigation bars visible!";

        if (DEBUG_LOGS) Log.i(TAG, "enterFullscreen, options=" + options.toString());
        WebContents webContents = tab.getWebContents();
        if (webContents == null) return;
        mFullscreenOptions = options;
        final View contentView = tab.getContentView();
        int systemUiVisibility = contentView.getSystemUiVisibility();
        if (isAlreadyInFullscreenOrNavigationHidden(systemUiVisibility)) {
            if (hasDesiredStatusBarAndNavigationState(systemUiVisibility, mFullscreenOptions)) {
                // We are already in fullscreen mode and the visibility flags match what we need;
                // nothing to do:
                return;
            }

            // Already in full screen mode; just changed options. Mask off old
            // ones and apply new ones.
            systemUiVisibility = applyExitFullscreenUIFlags(systemUiVisibility);
            systemUiVisibility = applyEnterFullscreenUIFlags(systemUiVisibility);
        } else if ((systemUiVisibility & SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN)
                        == SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN
                || (systemUiVisibility & SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION)
                        == SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION) {
            systemUiVisibility = applyEnterFullscreenUIFlags(systemUiVisibility);
        } else {
            Activity activity = TabUtils.getActivity(tab);
            boolean isMultiWindow = MultiWindowUtils.getInstance().isInMultiWindowMode(activity);

            // To avoid a double layout that is caused by the system when just hiding
            // the status bar set the status bar as translucent immediately. This causes
            // it not to take up space so the layout is stable. (See https://crbug.com/935015).
            // Do not do this in multi-window mode or automotive devices since the status bar is
            // forced to always be visible.
            if (!mFullscreenOptions.showStatusBar && !isMultiWindow
                    && !BuildInfo.getInstance().isAutomotive) {
                setWindowFlags(WindowManager.LayoutParams.FLAG_TRANSLUCENT_STATUS);
            }

            if (!mFullscreenOptions.showNavigationBar) {
                systemUiVisibility |= SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION;
            }

            if (!mFullscreenOptions.showStatusBar) {
                systemUiVisibility |= SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN;
            }
        }
        if (mFullscreenOnLayoutChangeListener != null) {
            contentView.removeOnLayoutChangeListener(mFullscreenOnLayoutChangeListener);
        }

        mFullscreenOnLayoutChangeListener = new OnLayoutChangeListener() {
            @Override
            public void onLayoutChange(View v, int left, int top, int right, int bottom,
                    int oldLeft, int oldTop, int oldRight, int oldBottom) {
                // On certain sites playing embedded video (http://crbug.com/293782), setting the
                // SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN does not always trigger a view-level layout
                // with an updated height.  To work around this, do not check for an increased
                // height and always just trigger the next step of the fullscreen initialization.
                // Posting the message to set the fullscreen flag because setting it directly in the
                // onLayoutChange would have no effect.
                mHandler.sendEmptyMessage(MSG_ID_SET_FULLSCREEN_SYSTEM_UI_FLAGS);

                if ((bottom - top) <= (oldBottom - oldTop) && (right - left) <= (oldRight - oldLeft)
                        && !BuildInfo.getInstance().isAutomotive) {
                    return;
                }

                getToast().onFullscreenLayout();
                contentView.removeOnLayoutChangeListener(this);
            }
        };

        contentView.addOnLayoutChangeListener(mFullscreenOnLayoutChangeListener);
        if (DEBUG_LOGS) Log.i(TAG, "enterFullscreen, systemUiVisibility=" + systemUiVisibility);
        setSystemUiVisibility(contentView, systemUiVisibility);

        // Request a layout so the updated system visibility takes affect.
        // The flow will continue in the handler of MSG_ID_SET_FULLSCREEN_SYSTEM_UI_FLAGS message.
        ViewUtils.requestLayout(contentView, "FullscreenHtmlApiHandler.enterFullScreen");

        mWebContentsInFullscreen = webContents;
        mContentViewInFullscreen = contentView;
        mTabInFullscreen = tab;
        getToast().onEnterFullscreen();
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

    // View.OnSystemUiVisibilityChangeListener

    @Override
    public void onSystemUiVisibilityChange(int visibility) {
        if (mTabInFullscreen == null || !getPersistentFullscreenMode()) return;
        mHandler.sendEmptyMessageDelayed(
                MSG_ID_SET_FULLSCREEN_SYSTEM_UI_FLAGS, ANDROID_CONTROLS_SHOW_DURATION_MS);
    }

    // WindowFocusChangedListener

    @Override
    public void onWindowFocusChanged(Activity activity, boolean hasWindowFocus) {
        if (mActivity != activity) return;

        // Window focus events can occur before the fullscreen toast is ready. It may skip and
        // wait till fullscreen is entered, by which time the toast object will be ready.
        if (mToast != null) mToast.onWindowFocusChanged(hasWindowFocus);

        mHandler.removeMessages(MSG_ID_SET_FULLSCREEN_SYSTEM_UI_FLAGS);
        mHandler.removeMessages(MSG_ID_CLEAR_LAYOUT_FULLSCREEN_FLAG);
        if (mTabInFullscreen == null || !getPersistentFullscreenMode() || !hasWindowFocus) return;
        mHandler.sendEmptyMessageDelayed(
                MSG_ID_SET_FULLSCREEN_SYSTEM_UI_FLAGS, ANDROID_CONTROLS_SHOW_DURATION_MS);
    }

    /*
     * Returns system ui flags to enable fullscreen mode based on the current options.
     * @return fullscreen flags to be applied to system UI visibility.
     */
    private int applyEnterFullscreenUIFlags(int systemUiVisibility) {
        boolean showNavigationBar =
                mFullscreenOptions != null ? mFullscreenOptions.showNavigationBar : false;
        boolean showStatusBar =
                mFullscreenOptions != null ? mFullscreenOptions.showStatusBar : false;

        int flags = SYSTEM_UI_FLAG_IMMERSIVE_STICKY;
        if (!showStatusBar && !showNavigationBar) {
            flags |= SYSTEM_UI_FLAG_LOW_PROFILE;
        }

        if (!showNavigationBar) {
            flags |= SYSTEM_UI_FLAG_HIDE_NAVIGATION;
            flags |= SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION;
        }

        if (!showStatusBar) {
            flags |= SYSTEM_UI_FLAG_FULLSCREEN;
            flags |= SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN;
        }

        return flags | systemUiVisibility;
    }

    /*
     * Returns system ui flags with any flags that might have been set during
     * applyEnterFullscreenUIFlags masked off.
     * @return fullscreen flags to be applied to system UI visibility.
     */
    private static int applyExitFullscreenUIFlags(int systemUiVisibility) {
        int maskOffFlags = SYSTEM_UI_FLAG_LOW_PROFILE | SYSTEM_UI_FLAG_FULLSCREEN
                | SYSTEM_UI_FLAG_HIDE_NAVIGATION | SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN
                | SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION | SYSTEM_UI_FLAG_IMMERSIVE_STICKY;

        return systemUiVisibility & ~maskOffFlags;
    }

    /*
     * Clears the current window attributes to not contain windowFlags. This
     * is slightly different that Window.clearFlags which then sets a
     * forced window attribute on the Window object that cannot be cleared.
     */
    private void clearWindowFlags(int windowFlags) {
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
    private void setWindowFlags(int windowFlags) {
        Window window = mActivity.getWindow();
        final WindowManager.LayoutParams attrs = window.getAttributes();
        attrs.flags |= windowFlags;
        window.setAttributes(attrs);
    }

    /**
     * Destroys the FullscreenHtmlApiHandler.
     */
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

    private static void setSystemUiVisibility(View contentView, int systemUiVisibility) {
        if (!BuildInfo.getInstance().isAutomotive) {
            contentView.setSystemUiVisibility(systemUiVisibility);
        }
    }
}
