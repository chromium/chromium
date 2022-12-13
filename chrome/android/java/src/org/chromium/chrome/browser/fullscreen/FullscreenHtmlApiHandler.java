// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.fullscreen;

import static android.view.View.SYSTEM_UI_FLAG_FULLSCREEN;
import static android.view.View.SYSTEM_UI_FLAG_HIDE_NAVIGATION;
import static android.view.View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN;
import static android.view.View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION;
import static android.view.View.SYSTEM_UI_FLAG_LOW_PROFILE;

import android.app.Activity;
import android.graphics.Rect;
import android.os.Handler;
import android.os.Message;
import android.view.LayoutInflater;
import android.view.View;
import android.view.View.OnLayoutChangeListener;
import android.view.ViewGroup;
import android.view.ViewPropertyAnimator;
import android.view.ViewTreeObserver.OnGlobalLayoutListener;
import android.view.Window;
import android.view.WindowManager;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;
import androidx.core.util.ObjectsCompat;

import org.chromium.base.ActivityState;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.ApplicationStatus.ActivityStateListener;
import org.chromium.base.ApplicationStatus.WindowFocusChangedListener;
import org.chromium.base.Log;
import org.chromium.base.ObserverList;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.cc.input.BrowserControlsState;
import org.chromium.chrome.R;
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
import org.chromium.chrome.browser.vr.VrModuleProvider;
import org.chromium.components.browser_ui.util.DimensionCompat;
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
    // Fade in/out animation duration for fullscreen notification toast.
    private static final int TOAST_FADE_MS = 500;
    // Time that the notification toast remains on-screen before starting to fade out.
    private static final int TOAST_SHOW_DURATION_MS = 5000;

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

    // Toast at the top of the screen that is shown when user enters fullscreen for the
    // first time.
    //
    // This is whether we believe that we need to show the user a notification toast.  It's false if
    // we're not in full screen, or if we are in full screen but have already shown the toast for
    // enough time for the user to read it.  The toast might or might not actually be on-screen
    // right now; we remove it in some cases like when we lose window focus.  However, as long as
    // we'll be in full screen, we still keep the toast pending until we successfully show it.
    private boolean mIsNotificationToastPending;

    // Sometimes, the toast must be removed temporarily, such as when we lose focus or if we
    // transition to picture-in-picture.  In those cases, the toast is removed from the view
    // hierarchy, and these fields are cleared.  The toast will be re-created from scratch when it's
    // appropriate to show it again.  `mIsNotificationToastPending` won't be reset in those cases,
    // though, since we'll still want to show the toast when it's possible to do so.
    //
    // If `mNotificationToast` exists, then it's attached to the view hierarchy, though it might be
    // animating to or from alpha=0.  Any time the toast exists, we also have an animation for it,
    // to allow us to fade it in, and eventually back out.  The animation is not cleared when it
    // completes; it's only cleared when we also detach the toast and clear `mNotificationToast`.
    //
    // Importantly, it's possible that `mNotificationToast` is not null while no toast is pending.
    // This can happen when the toast has been on-screen long enough, and is fading out.
    private View mNotificationToast;
    private ViewPropertyAnimator mToastFadeAnimation;

    // Runnable that will complete the current toast and fade it out.
    private final Runnable mFadeOutNotificationToastRunnable;

    private OnLayoutChangeListener mFullscreenOnLayoutChangeListener;

    private FullscreenOptions mPendingFullscreenOptions;

    private ActivityTabTabObserver mActiveTabObserver;
    private TabModelSelectorTabObserver mTabFullscreenObserver;
    @Nullable
    private Tab mTab;

    // Current ContentView. Updates when active tab is switched or WebContents is swapped
    // in the current Tab.
    private ContentView mContentView;

    private DimensionCompat mDimensionCompat;
    private int mNavbarHeight;

    // Monitors the window layout change while the fullscreen toast is on.
    private OnGlobalLayoutListener mWindowLayoutListener = new OnGlobalLayoutListener() {
        @Override
        public void onGlobalLayout() {
            if (mContentViewInFullscreen == null || mNotificationToast == null) return;
            Rect bounds = new Rect();
            mContentViewInFullscreen.getWindowVisibleDisplayFrame(bounds);
            var lp = (ViewGroup.MarginLayoutParams) mNotificationToast.getLayoutParams();
            int bottomMargin = mContentViewInFullscreen.getHeight() - bounds.height();
            // If positioned at the bottom of the display, shift it up to avoid overlapping
            // with the bottom nav bar when it appears by user gestures.
            if (bottomMargin == 0) bottomMargin = mNavbarHeight;
            lp.setMargins(0, 0, 0, bottomMargin);
            mNotificationToast.requestLayout();
        }
    };

    // This static inner class holds a WeakReference to the outer object, to avoid triggering the
    // lint HandlerLeak warning.
    private static class FullscreenHandler extends Handler {
        private final WeakReference<FullscreenHtmlApiHandler> mFullscreenHtmlApiHandler;

        public FullscreenHandler(FullscreenHtmlApiHandler fullscreenHtmlApiHandler) {
            mFullscreenHtmlApiHandler = new WeakReference<FullscreenHtmlApiHandler>(
                    fullscreenHtmlApiHandler);
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
                        contentView.setSystemUiVisibility(systemUiVisibility);
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
                        public void onLayoutChange(View v, int left, int top, int right,
                                int bottom, int oldLeft, int oldTop, int oldRight,
                                int oldBottom) {
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
                    contentView.setSystemUiVisibility(systemUiVisibility);
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
        mFadeOutNotificationToastRunnable = this::fadeOutNotificationToast;
        mDimensionCompat = DimensionCompat.create(mActivity, () -> {});
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
            public void onDidFinishNavigationNoop(Tab tab, NavigationHandle navigation) {
                if (!navigation.isInPrimaryMainFrame()) return;
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
        if (wasInPersistentFullscreenMode) {
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
            cancelNotificationToast();
            mPersistentModeSupplier.set(false);

            if (mWebContentsInFullscreen != null && mTabInFullscreen != null) {
                exitFullscreen(
                        mWebContentsInFullscreen, mContentViewInFullscreen, mTabInFullscreen);
            } else {
                assert mPendingFullscreenOptions
                        != null : "No content previously set to fullscreen.";
                mPendingFullscreenOptions.setCanceled();
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
        cancelNotificationToast();
        mHandler.removeMessages(MSG_ID_SET_FULLSCREEN_SYSTEM_UI_FLAGS);
        mHandler.removeMessages(MSG_ID_CLEAR_LAYOUT_FULLSCREEN_FLAG);

        int systemUiVisibility = contentView.getSystemUiVisibility();
        systemUiVisibility = applyExitFullscreenUIFlags(systemUiVisibility);
        clearWindowFlags(WindowManager.LayoutParams.FLAG_TRANSLUCENT_STATUS);
        if (DEBUG_LOGS) Log.i(TAG, "exitFullscreen, systemUiVisibility=" + systemUiVisibility);
        contentView.setSystemUiVisibility(systemUiVisibility);
        if (mFullscreenOnLayoutChangeListener != null) {
            contentView.removeOnLayoutChangeListener(mFullscreenOnLayoutChangeListener);
        }
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
            // it not to take up space so the layout is stable. (See https://crbug.com/935015). Do
            // not do this in multi-window mode since that mode forces the status bar
            // to always be visible.
            if (!mFullscreenOptions.showStatusBar && !isMultiWindow) {
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

                if ((bottom - top) <= (oldBottom - oldTop)
                        && (right - left) <= (oldRight - oldLeft)) {
                    return;
                }

                beginNotificationToast();
                contentView.removeOnLayoutChangeListener(this);
            }
        };

        contentView.addOnLayoutChangeListener(mFullscreenOnLayoutChangeListener);
        if (DEBUG_LOGS) Log.i(TAG, "enterFullscreen, systemUiVisibility=" + systemUiVisibility);
        contentView.setSystemUiVisibility(systemUiVisibility);

        // Request a layout so the updated system visibility takes affect.
        // The flow will continue in the handler of MSG_ID_SET_FULLSCREEN_SYSTEM_UI_FLAGS message.
        ViewUtils.requestLayout(contentView, "FullscreenHtmlApiHandler.enterFullScreen");

        mWebContentsInFullscreen = webContents;
        mContentViewInFullscreen = contentView;
        mTabInFullscreen = tab;

        // Cache the navigation bar height before entering fullscreen mode in which the dimension
        // is zero.
        mNavbarHeight = mDimensionCompat.getNavbarHeight();
        mActivity.getWindow().getDecorView().getViewTreeObserver().addOnGlobalLayoutListener(
                mWindowLayoutListener);
    }

    /**
     * Whether we show a toast message when entering fullscreen.
     */
    private boolean shouldShowToast() {
        // If there's no notification toast pending, such as when we're not in full screen or after
        // we've already displayed it for longe enough, then we don't need to show the toast now.
        if (!mIsNotificationToastPending) return false;

        if (mTabInFullscreen == null) return false;

        if (mTab == null) return false;

        // The toast tells user how to leave fullscreen by touching the screen. Currently
        // we do not show the toast when we're browsing in VR, since VR doesn't have
        // touchscreen and the toast doesn't have any useful information.
        if (VrModuleProvider.getDelegate().isInVr() || VrModuleProvider.getDelegate().bootsToVr()) {
            return false;
        }

        final ViewGroup parent = mTab.getContentView();
        if (parent == null) return false;

        // The window must have the focus, so that it is not obscured while the notification is
        // showing.  This also covers the case of picture in picture video, but any case of an
        // unfocused window should prevent the toast.
        if (!parent.hasWindowFocus()) return false;

        return true;
    }

    /**
     * Create and show the fullscreen notification toast, if it's not already visible and if it
     * should be visible.  It's okay to call this when it should not be; we'll do nothing.  This
     * will fade the toast in if needed.  It will also schedule a timer to fade it back out, if it's
     * not hidden or cancelled before then.
     */
    private void createAndShowNotificationToast() {
        // If it's already visible, then that's fine.  That includes if it's currently fading out;
        // that's part of it.
        if (mNotificationToast != null) return;

        // If the toast should not be visible, then do nothing.
        if (!shouldShowToast()) return;

        assert mTab != null && mTab.getContentView() != null;

        // Create a new toast and fade it in, or re-use one we've created before.
        mNotificationToast = mActivity.getWindow().findViewById(R.id.fullscreen_notification);
        boolean addView = false;
        if (mNotificationToast == null) {
            mNotificationToast =
                    LayoutInflater.from(mActivity).inflate(R.layout.fullscreen_notification, null);
            addView = true;
        }
        mNotificationToast.setAlpha(0);
        mToastFadeAnimation = mNotificationToast.animate();
        if (addView) {
            mActivity.addContentView(mNotificationToast,
                    new ViewGroup.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT,
                            ViewGroup.LayoutParams.MATCH_PARENT));
            // Ensure the toast is visible on bottom sheet CCT which is elevated for shadow effect.
            // Does no harm on other embedders.
            mNotificationToast.setElevation(mActivity.getResources().getDimensionPixelSize(
                    R.dimen.fullscreen_toast_elevation));
        } else {
            mNotificationToast.setVisibility(View.VISIBLE);
        }

        mToastFadeAnimation.alpha(1).setDuration(TOAST_FADE_MS).start();
        mHandler.postDelayed(mFadeOutNotificationToastRunnable, TOAST_SHOW_DURATION_MS);
    }

    /**
     * Pause the notification toast, which hides it and stops all the timers.  It's okay if there is
     * not currently a toast; we don't change any state in that case.  This will abruptly hide the
     * toast, rather than fade it out.  This does not change `mIsNotificationToastPending`; the
     * toast hasn't been shown long enough.
     */
    private void hideImmediatelyNotificationToast() {
        if (mNotificationToast == null) return;

        // Stop the fade-out timer.
        mHandler.removeCallbacks(mFadeOutNotificationToastRunnable);

        // Remove it immediately, without fading out.
        assert mToastFadeAnimation != null;
        mToastFadeAnimation.cancel();
        mToastFadeAnimation = null;

        mActivity.getWindow().getDecorView().getViewTreeObserver().removeOnGlobalLayoutListener(
                mWindowLayoutListener);

        // We can't actually remove it, so this will do.
        mNotificationToast.setVisibility(View.GONE);
        mNotificationToast = null;
    }

    /**
     * Begin a new instance of the notification toast.  If the toast should not be shown right now,
     * we'll start showing it when we can.
     */
    private void beginNotificationToast() {
        // It would be nice if we could determine that we're not starting a new toast while a
        // previous one is fading out.  We can't ask the animation for its current target value.  We
        // could almost check that there's not a notification pending and also that there's no
        // current toast.  When a notification is pending, the previous toast hasn't completed yet,
        // so nobody should be starting a new one.  When `mNotificationToast` is not null, but
        // pending is false, then the fade-out animation has started but not completed.  Only when
        // they're both false is it in the steady-state of "no notification" that would let us start
        // a new one.
        //
        // The problem with that is that there are cases when we double-enter fullscreen.  In
        // particular, changing the visibility of the navigation bar and/or status bar can cause us
        // to think that we're entering fullscreen without an intervening exit.  In this case, the
        // right thing to do is to continue with the toast from the previous full screen, if it's
        // still on-screen.  If it's fading out now, just let it continue to fade out.  The user has
        // already seen it for the full duration, and we've not actually exited fullscreen.
        if (mNotificationToast != null) {
            // Don't reset the pending flag here -- either it's on the screen or fading out, and
            // either way is correct.  We have not actually exited fullscreen, so we shouldn't
            // re-display the notification.
            return;
        }

        mIsNotificationToastPending = true;
        createAndShowNotificationToast();
    }

    /**
     * Cancel a toast immediately, without fading out.  For example, if we leave fullscreen, then
     * the toast isn't needed anymore.
     */
    private void cancelNotificationToast() {
        hideImmediatelyNotificationToast();
        // Don't restart it either.
        mIsNotificationToastPending = false;
    }

    /**
     * Called when the notification toast should not be shown any more, because it's been on-screen
     * long enough for the user to read it.  To re-show it, one must call `beginNotificationToast()`
     * again.  Show / hide of the toast will no-op until then.
     */
    private void fadeOutNotificationToast() {
        if (mNotificationToast == null) return;

        // Clear this first, so that we know that the toast timer has expired already.
        mIsNotificationToastPending = false;

        // Cancel any timer that will start the fade-out animation, in case it's running.  It might
        // not be, especially if we're called by it.
        mHandler.removeCallbacks(mFadeOutNotificationToastRunnable);

        // Start the fade-out animation.
        assert mToastFadeAnimation != null;
        mToastFadeAnimation.cancel();
        mToastFadeAnimation.alpha(0)
                .setDuration(TOAST_FADE_MS)
                .withEndAction(this::hideImmediatelyNotificationToast);
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

        // Try to show / hide the toast, if we need to.  Note that these won't do anything if the
        // toast should not be visible, such as if we re-gain the window focus after having
        // completed the most recently started notification toast.
        //
        // Also note that this handles picture-in-picture.  We definitely do not want the toast to
        // be visible then; it's not relevant and also takes up almost all of the window.  We could
        // also do this on ActivityStateChanged => PAUSED if Activity.isInPictureInPictureMode(),
        // but it doesn't seem to be needed.
        if (hasWindowFocus) {
            createAndShowNotificationToast();
        } else {
            // While we don't have the focus, hide any ongoing notification.
            hideImmediatelyNotificationToast();
        }

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

        int flags = View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY;
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
                | SYSTEM_UI_FLAG_HIDE_NAVIGATION;
        maskOffFlags |= SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN | SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION;
        maskOffFlags |= View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY;

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

    boolean isToastVisibleForTesting() {
        return mNotificationToast != null;
    }

    int getToastBottomMarginForTesting() {
        var lp = (ViewGroup.MarginLayoutParams) mNotificationToast.getLayoutParams();
        return lp.bottomMargin;
    }

    void setVersionCompatForTesting(DimensionCompat compat) {
        mDimensionCompat = compat;
    }

    void triggerWindowLayoutChangeForTesting() {
        mWindowLayoutListener.onGlobalLayout();
    }
}
