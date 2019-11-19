// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.fullscreen;

import static android.view.View.SYSTEM_UI_FLAG_FULLSCREEN;
import static android.view.View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN;
import static android.view.View.SYSTEM_UI_FLAG_LOW_PROFILE;

import android.app.Activity;
import android.os.Build;
import android.os.Handler;
import android.os.Message;
import android.support.v4.util.ObjectsCompat;
import android.view.Gravity;
import android.view.View;
import android.view.View.OnLayoutChangeListener;
import android.view.Window;
import android.view.WindowManager;

import androidx.annotation.Nullable;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.multiwindow.MultiWindowUtils;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.widget.Toast;

import java.lang.ref.WeakReference;

/**
 * Handles updating the UI based on requests to the HTML Fullscreen API.
 */
public class FullscreenHtmlApiHandler {
    private static final int MSG_ID_SET_FULLSCREEN_SYSTEM_UI_FLAGS = 1;
    private static final int MSG_ID_CLEAR_LAYOUT_FULLSCREEN_FLAG = 2;

    // The time we allow the Android notification bar to be shown when it is requested temporarily
    // by the Android system (this value is additive on top of the show duration imposed by
    // Android).
    private static final long ANDROID_CONTROLS_SHOW_DURATION_MS = 200;
    // Delay to allow a frame to render between getting the fullscreen layout update and clearing
    // the SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN flag.
    private static final long CLEAR_LAYOUT_FULLSCREEN_DELAY_MS = 20;

    private final Window mWindow;
    private final Handler mHandler;
    private final FullscreenHtmlApiDelegate mDelegate;

    // We need to cache WebContents/ContentView since we are setting fullscreen UI state on
    // the WebContents's container view, and a Tab can change to have null web contents/
    // content view, i.e., if you navigate to a native page.
    @Nullable
    private WebContents mWebContentsInFullscreen;
    @Nullable
    private View mContentViewInFullscreen;
    @Nullable private Tab mTabInFullscreen;
    private boolean mIsPersistentMode;
    private FullscreenOptions mFullscreenOptions;

    // Toast at the top of the screen that is shown when user enters fullscreen for the
    // first time.
    private Toast mNotificationToast;

    private OnLayoutChangeListener mFullscreenOnLayoutChangeListener;

    /**
     * Delegate that allows embedders to react to fullscreen API requests.
     */
    public interface FullscreenHtmlApiDelegate {
        /**
         * Notifies the delegate that entering fullscreen has been requested and allows them
         * to hide their controls.
         * <p>
         * Once the delegate has hidden the their controls, it must call
         * {@link FullscreenHtmlApiHandler#enterFullscreen(Tab)}.
         */
        void onEnterFullscreen(FullscreenOptions options);

        /**
         * Cancels a pending enter fullscreen request if present.
         * @return Whether the request was cancelled.
         */
        boolean cancelPendingEnterFullscreen();

        /**
         * Notifies the delegate that the window UI has fully exited fullscreen and gives
         * the embedder a chance to update their controls.
         *
         * @param tab The tab whose fullscreen is being exited.
         */
        void onFullscreenExited(Tab tab);

        /**
         * @return Whether the notification toast should be shown. For fullscreen video in
         *         overlay mode, the notification toast should be disabled.
         */
        boolean shouldShowNotificationToast();
    }

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
                    // assert mIsPersistentMode : "Calling after we exited fullscreen";
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
     * @param window The window containing the view going to fullscreen.
     * @param delegate The delegate that allows embedders to handle fullscreen transitions.
     */
    public FullscreenHtmlApiHandler(Window window, FullscreenHtmlApiDelegate delegate) {
        mWindow = window;
        mDelegate = delegate;
        mHandler = new FullscreenHandler(this);
    }

    /**
     * Enters persistent fullscreen mode. In this mode, the browser controls will be
     * permanently hidden until this mode is exited.
     *
     * @param options Options to choose mode of fullscreen.
     */
    public void enterPersistentFullscreenMode(FullscreenOptions options) {
        if (mIsPersistentMode && ObjectsCompat.equals(mFullscreenOptions, options)) {
            return;
        }

        mIsPersistentMode = true;
        mDelegate.onEnterFullscreen(options);
    }

    /**
     * Exits persistent fullscreen mode. Will restore browser controls visibility
     * if they have been hidden.
     */
    public void exitPersistentFullscreenMode() {
        if (!mIsPersistentMode) return;

        mIsPersistentMode = false;

        if (mWebContentsInFullscreen != null && mTabInFullscreen != null) {
            exitFullscreen(mWebContentsInFullscreen, mContentViewInFullscreen, mTabInFullscreen);
        } else {
            if (!mDelegate.cancelPendingEnterFullscreen()) {
                assert false : "No content view previously set to fullscreen.";
            }
        }
        mWebContentsInFullscreen = null;
        mContentViewInFullscreen = null;
        mTabInFullscreen = null;
        mFullscreenOptions = null;
    }

    /**
     * @return Whether the application is in persistent fullscreen mode.
     * @see #setPersistentFullscreenMode(boolean)
     */
    public boolean getPersistentFullscreenMode() {
        return mIsPersistentMode;
    }

    private void exitFullscreen(WebContents webContents, View contentView, Tab tab) {
        hideNotificationToast();
        mHandler.removeMessages(MSG_ID_SET_FULLSCREEN_SYSTEM_UI_FLAGS);
        mHandler.removeMessages(MSG_ID_CLEAR_LAYOUT_FULLSCREEN_FLAG);

        int systemUiVisibility = contentView.getSystemUiVisibility();
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.JELLY_BEAN_MR2) {
            systemUiVisibility &= ~SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN;
            systemUiVisibility = applyExitFullscreenUIFlags(systemUiVisibility);
            clearWindowFlags(WindowManager.LayoutParams.FLAG_TRANSLUCENT_STATUS);
        } else {
            systemUiVisibility &= ~SYSTEM_UI_FLAG_LOW_PROFILE;
            mWindow.addFlags(WindowManager.LayoutParams.FLAG_FORCE_NOT_FULLSCREEN);
            mWindow.clearFlags(WindowManager.LayoutParams.FLAG_FULLSCREEN);
        }
        contentView.setSystemUiVisibility(systemUiVisibility);
        if (mFullscreenOnLayoutChangeListener != null) {
            contentView.removeOnLayoutChangeListener(mFullscreenOnLayoutChangeListener);
        }
        mFullscreenOnLayoutChangeListener = new OnLayoutChangeListener() {
            @Override
            public void onLayoutChange(View v, int left, int top, int right, int bottom,
                    int oldLeft, int oldTop, int oldRight, int oldBottom) {
                if ((bottom - top) < (oldBottom - oldTop)) {
                    mDelegate.onFullscreenExited(tab);
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
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.JELLY_BEAN_MR2) {
            if ((systemUiVisibility & SYSTEM_UI_FLAG_FULLSCREEN) == SYSTEM_UI_FLAG_FULLSCREEN) {
                // Already in full screen mode; just changed options. Mask off old
                // ones and apply new ones.
                systemUiVisibility = applyExitFullscreenUIFlags(systemUiVisibility);
                systemUiVisibility = applyEnterFullscreenUIFlags(systemUiVisibility);
            } else if ((systemUiVisibility & SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN)
                    == SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN) {
                systemUiVisibility = applyEnterFullscreenUIFlags(systemUiVisibility);
            } else {
                Activity activity = tab.getActivity();
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
        } else {
            systemUiVisibility |= SYSTEM_UI_FLAG_LOW_PROFILE;
            mWindow.addFlags(WindowManager.LayoutParams.FLAG_FULLSCREEN);
            mWindow.clearFlags(WindowManager.LayoutParams.FLAG_FORCE_NOT_FULLSCREEN);
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
                if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.JELLY_BEAN_MR2) {
                    // Posting the message to set the fullscreen flag because setting it
                    // directly in the onLayoutChange would have no effect.
                    mHandler.sendEmptyMessage(MSG_ID_SET_FULLSCREEN_SYSTEM_UI_FLAGS);
                }

                if ((bottom - top) <= (oldBottom - oldTop)) return;
                if (mDelegate.shouldShowNotificationToast()) {
                    showNotificationToast();
                }
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
     * Create and show the fullscreen notification toast.
     */
    private void showNotificationToast() {
        if (mNotificationToast == null) {
            int resId = (Build.VERSION.SDK_INT >= Build.VERSION_CODES.KITKAT)
                    ? R.string.immersive_fullscreen_api_notification
                    : R.string.fullscreen_api_notification;
            mNotificationToast = Toast.makeText(
                    mWindow.getContext(), resId, Toast.LENGTH_LONG);
            mNotificationToast.setGravity(Gravity.TOP | Gravity.CENTER, 0, 0);
        }
        mNotificationToast.show();
    }

    /**
     * Hides the notification toast.
     */
    public void hideNotificationToast() {
        if (mNotificationToast != null) {
            mNotificationToast.cancel();
        }
    }

    /**
     * Notified when the system UI visibility for the current ContentView has changed.
     * @param visibility The updated UI visibility.
     * @see View#getSystemUiVisibility()
     */
    public void onContentViewSystemUiVisibilityChange(int visibility) {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.JELLY_BEAN_MR2) return;

        if (mTabInFullscreen == null || !mIsPersistentMode) return;
        mHandler.sendEmptyMessageDelayed(
                MSG_ID_SET_FULLSCREEN_SYSTEM_UI_FLAGS, ANDROID_CONTROLS_SHOW_DURATION_MS);
    }

    /**
     * Ensure the proper system UI flags are set after the window regains focus.
     * @see android.app.Activity#onWindowFocusChanged(boolean)
     */
    public void onWindowFocusChanged(boolean hasWindowFocus) {
        if (!hasWindowFocus) hideNotificationToast();
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.JELLY_BEAN_MR2) return;

        mHandler.removeMessages(MSG_ID_SET_FULLSCREEN_SYSTEM_UI_FLAGS);
        mHandler.removeMessages(MSG_ID_CLEAR_LAYOUT_FULLSCREEN_FLAG);
        if (mTabInFullscreen == null || !mIsPersistentMode || !hasWindowFocus) return;
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
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.KITKAT) {
            flags |= View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY;
        }
        if (!showNavigationBar) {
            flags |= SYSTEM_UI_FLAG_LOW_PROFILE;
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.KITKAT) {
                flags |= View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION;
                flags |= View.SYSTEM_UI_FLAG_HIDE_NAVIGATION;
            }
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
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.KITKAT) {
            maskOffFlags |= View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION;
            maskOffFlags |= View.SYSTEM_UI_FLAG_HIDE_NAVIGATION;
            maskOffFlags |= View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY;
        }
        return systemUiVisibility & ~maskOffFlags;
    }

    /*
     * Clears the current window attributes to not contain windowFlags. This
     * is slightly different that mWindow.clearFlags which then sets a
     * forced window attribute on the Window object that cannot be cleared.
     */
    private void clearWindowFlags(int windowFlags) {
        final WindowManager.LayoutParams attrs = mWindow.getAttributes();
        if ((attrs.flags & windowFlags) != 0) {
            attrs.flags &= ~windowFlags;
            mWindow.setAttributes(attrs);
        }
    }

    /*
     * Sets the current window attributes to contain windowFlags. This
     * is slightly different that mWindow.setFlags which then sets a
     * forced window attribute on the Window object that cannot be cleared.
     */
    private void setWindowFlags(int windowFlags) {
        final WindowManager.LayoutParams attrs = mWindow.getAttributes();
        attrs.flags |= windowFlags;
        mWindow.setAttributes(attrs);
    }
}
