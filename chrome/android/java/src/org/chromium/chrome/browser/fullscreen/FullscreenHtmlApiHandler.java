// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.fullscreen;

import static android.view.View.SYSTEM_UI_FLAG_FULLSCREEN;
import static android.view.View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN;
import static android.view.View.SYSTEM_UI_FLAG_LOW_PROFILE;

import android.app.Activity;
import android.os.Handler;
import android.os.Message;
import android.view.Gravity;
import android.view.View;
import android.view.View.OnLayoutChangeListener;
import android.view.Window;
import android.view.WindowManager;

import androidx.annotation.Nullable;
import androidx.core.util.ObjectsCompat;

import org.chromium.base.ActivityState;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.ApplicationStatus.ActivityStateListener;
import org.chromium.base.ApplicationStatus.WindowFocusChangedListener;
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
import org.chromium.components.embedder_support.view.ContentView;
import org.chromium.content_public.browser.GestureListenerManager;
import org.chromium.content_public.browser.NavigationHandle;
import org.chromium.content_public.browser.SelectionPopupController;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.widget.Toast;

import java.lang.ref.WeakReference;

/**
 * Handles updating the UI based on requests to the HTML Fullscreen API.
 */
public class FullscreenHtmlApiHandler implements ActivityStateListener, WindowFocusChangedListener,
                                                 View.OnSystemUiVisibilityChangeListener,
                                                 FullscreenManager {
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
    @Nullable private Tab mTabInFullscreen;
    private FullscreenOptions mFullscreenOptions;

    // Toast at the top of the screen that is shown when user enters fullscreen for the
    // first time.
    private Toast mNotificationToast;

    private OnLayoutChangeListener mFullscreenOnLayoutChangeListener;

    private FullscreenOptions mPendingFullscreenOptions;

    private ActivityTabTabObserver mActiveTabObserver;
    private TabModelSelectorTabObserver mTabFullscreenObserver;
    @Nullable
    private Tab mTab;

    // Current ContentView. Updates when active tab is switched or WebContents is swapped
    // in the current Tab.
    private ContentView mContentView;

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

                    if ((systemUiVisibility & SYSTEM_UI_FLAG_FULLSCREEN)
                            == SYSTEM_UI_FLAG_FULLSCREEN) {
                        return;
                    }
                    systemUiVisibility = fullscreenHtmlApiHandler.applyEnterFullscreenUIFlags(
                            systemUiVisibility);
                    contentView.setSystemUiVisibility(systemUiVisibility);

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

                    contentView.requestLayout();
                    break;
                }
                case MSG_ID_CLEAR_LAYOUT_FULLSCREEN_FLAG: {
                    // Change this assert to simply ignoring the message to work around
                    // https://crbug/365638
                    // TODO(aberent): Fix bug
                    // assert getPersistentFullscreenMode() : "Calling after we exited fullscreen";
                    if (!fullscreenHtmlApiHandler.getPersistentFullscreenMode()) return;

                    if ((systemUiVisibility & SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN) == 0) {
                        return;
                    }
                    systemUiVisibility &= ~SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN;
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
            public void onDidFinishNavigation(Tab tab, NavigationHandle navigation) {
                if (navigation.isInMainFrame() && !navigation.isSameDocument()) {
                    if (tab == modelSelector.getCurrentTab()) exitPersistentFullscreenMode();
                }
            }

            @Override
            public void onInteractabilityChanged(Tab tab, boolean interactable) {
                // Compare |tab| with |TabModelSelector#getCurrentTab()| which is a safer
                // indicator for the active tab than |mTab|, since the invocation order of
                // ActivityTabTabObserver and TabModelSelectorTabObserver is not explicitly defined.
                if (!interactable || tab != modelSelector.getCurrentTab()) return;
                Runnable enterFullscreen = getEnterFullscreenRunnable(tab);
                if (enterFullscreen != null) enterFullscreen.run();
            }
        };
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
        // If enabling fullscreen while the tab is not interactable, fullscreen
        // will be delayed until the tab is interactable.
        Runnable r = () -> {
            enterPersistentFullscreenMode(options);
            destroySelectActionMode(tab);
            setEnterFullscreenRunnable(tab, null);
        };

        if (tab.isUserInteractable()) {
            r.run();
        } else {
            setEnterFullscreenRunnable(tab, r);
        }

        for (FullscreenManager.Observer observer : mObservers) {
            observer.onEnterFullscreen(tab, options);
        }
    }

    @Override
    public void onExitFullscreen(Tab tab) {
        setEnterFullscreenRunnable(tab, null);
        if (tab == mTab) exitPersistentFullscreenMode();
        for (FullscreenManager.Observer observer : mObservers) {
            observer.onExitFullscreen(tab);
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

    private void destroySelectActionMode(Tab tab) {
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

    private Runnable getEnterFullscreenRunnable(Tab tab) {
        return tab != null ? TabAttributes.from(tab).get(TabAttributeKeys.ENTER_FULLSCREEN) : null;
    }

    /**
     * Enters persistent fullscreen mode. In this mode, the browser controls will be
     * permanently hidden until this mode is exited.
     *
     * @param options Options to choose mode of fullscreen.
     */
    private void enterPersistentFullscreenMode(FullscreenOptions options) {
        if (!getPersistentFullscreenMode() || !ObjectsCompat.equals(mFullscreenOptions, options)) {
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
        if (!controlsHidden) return;
        if (mTab != null && mPendingFullscreenOptions != null) {
            enterFullscreen(mTab, mPendingFullscreenOptions);
            mPendingFullscreenOptions = null;
        }
    }

    @Override
    public void exitPersistentFullscreenMode() {
        if (getPersistentFullscreenMode()) {
            mPersistentModeSupplier.set(false);

            if (mWebContentsInFullscreen != null && mTabInFullscreen != null) {
                exitFullscreen(
                        mWebContentsInFullscreen, mContentViewInFullscreen, mTabInFullscreen);
            } else {
                assert mPendingFullscreenOptions
                        != null : "No content previously set to fullscreen.";
                mPendingFullscreenOptions = null;
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
    public ObservableSupplier<Boolean> getPersistentFullscreenModeSupplier() {
        return mPersistentModeSupplier;
    }

    private void exitFullscreen(WebContents webContents, View contentView, Tab tab) {
        hideNotificationToast();
        mHandler.removeMessages(MSG_ID_SET_FULLSCREEN_SYSTEM_UI_FLAGS);
        mHandler.removeMessages(MSG_ID_CLEAR_LAYOUT_FULLSCREEN_FLAG);

        int systemUiVisibility = contentView.getSystemUiVisibility();
        systemUiVisibility &= ~SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN;
        systemUiVisibility = applyExitFullscreenUIFlags(systemUiVisibility);
        clearWindowFlags(WindowManager.LayoutParams.FLAG_TRANSLUCENT_STATUS);
        contentView.setSystemUiVisibility(systemUiVisibility);
        if (mFullscreenOnLayoutChangeListener != null) {
            contentView.removeOnLayoutChangeListener(mFullscreenOnLayoutChangeListener);
        }
        mFullscreenOnLayoutChangeListener = new OnLayoutChangeListener() {
            @Override
            public void onLayoutChange(View v, int left, int top, int right, int bottom,
                    int oldLeft, int oldTop, int oldRight, int oldBottom) {
                if ((bottom - top) < (oldBottom - oldTop)) {
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

    /**
     * Handles hiding the system UI components to allow the content to take up the full screen.
     * @param tab The tab that is entering fullscreen.
     */
    public void enterFullscreen(final Tab tab, FullscreenOptions options) {
        WebContents webContents = tab.getWebContents();
        if (webContents == null) return;
        mFullscreenOptions = options;
        final View contentView = tab.getContentView();
        int systemUiVisibility = contentView.getSystemUiVisibility();
        if ((systemUiVisibility & SYSTEM_UI_FLAG_FULLSCREEN) == SYSTEM_UI_FLAG_FULLSCREEN) {
            // Already in full screen mode; just changed options. Mask off old
            // ones and apply new ones.
            systemUiVisibility = applyExitFullscreenUIFlags(systemUiVisibility);
            systemUiVisibility = applyEnterFullscreenUIFlags(systemUiVisibility);
        } else if ((systemUiVisibility & SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN)
                == SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN) {
            systemUiVisibility = applyEnterFullscreenUIFlags(systemUiVisibility);
        } else {
            Activity activity = TabUtils.getActivity(tab);
            boolean isMultiWindow = MultiWindowUtils.getInstance().isLegacyMultiWindow(activity)
                    || MultiWindowUtils.getInstance().isInMultiWindowMode(activity);

            // To avoid a double layout that is caused by the system when just hiding
            // the status bar set the status bar as translucent immediately. This cause
            // it not to take up space so the layout is stable. (See crbug.com/935015). Do
            // not do this in multi-window mode since that mode forces the status bar
            // to always be visible.
            if (mFullscreenOptions != null && mFullscreenOptions.showNavigationBar()
                    && !isMultiWindow) {
                setWindowFlags(WindowManager.LayoutParams.FLAG_TRANSLUCENT_STATUS);
            }
            systemUiVisibility |= SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN;
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

                if ((bottom - top) <= (oldBottom - oldTop)) return;

                // The toast tells user how to leave fullscreen by touching the screen. Currently
                // we do not show the toast when we're browsing in VR, since VR doesn't have
                // touchscreen and the toast doesn't have any useful information.
                if (shouldShowToast()) showNotificationToast();
                contentView.removeOnLayoutChangeListener(this);
            }
        };
        contentView.addOnLayoutChangeListener(mFullscreenOnLayoutChangeListener);
        contentView.setSystemUiVisibility(systemUiVisibility);
        mFullscreenOptions = options;

        // Request a layout so the updated system visibility takes affect.
        contentView.requestLayout();

        mWebContentsInFullscreen = webContents;
        mContentViewInFullscreen = contentView;
        mTabInFullscreen = tab;
    }

    /**
     * Whether we show a toast message when entering fullscreen.
     */
    private boolean shouldShowToast() {
        return !(VrModuleProvider.getDelegate().isInVr()
                || VrModuleProvider.getDelegate().bootsToVr());
    }

    /**
     * Create and show the fullscreen notification toast.
     */
    private void showNotificationToast() {
        if (mNotificationToast != null) {
            mNotificationToast.cancel();
        }
        int resId = R.string.immersive_fullscreen_api_notification;
        mNotificationToast = Toast.makeText(mActivity, resId, Toast.LENGTH_LONG);
        mNotificationToast.setGravity(Gravity.TOP | Gravity.CENTER, 0, 0);
        mNotificationToast.show();
    }

    /**
     * Hides the notification toast.
     */
    private void hideNotificationToast() {
        if (mNotificationToast != null) {
            mNotificationToast.cancel();
            mNotificationToast = null;
        }
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
        if (!hasWindowFocus) hideNotificationToast();

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
                mFullscreenOptions != null ? mFullscreenOptions.showNavigationBar() : false;
        int flags = SYSTEM_UI_FLAG_FULLSCREEN;
        flags |= View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY;
        if (!showNavigationBar) {
            flags |= SYSTEM_UI_FLAG_LOW_PROFILE;
            flags |= View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION;
            flags |= View.SYSTEM_UI_FLAG_HIDE_NAVIGATION;
        }
        return flags | systemUiVisibility;
    }

    /*
     * Returns system ui flags with any flags that might have been set during
     * applyEnterFullscreenUIFlags masked off.
     * @return fullscreen flags to be applied to system UI visibility.
     */
    private static int applyExitFullscreenUIFlags(int systemUiVisibility) {
        int maskOffFlags = SYSTEM_UI_FLAG_LOW_PROFILE | SYSTEM_UI_FLAG_FULLSCREEN;
        maskOffFlags |= View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION;
        maskOffFlags |= View.SYSTEM_UI_FLAG_HIDE_NAVIGATION;
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
    }
}
