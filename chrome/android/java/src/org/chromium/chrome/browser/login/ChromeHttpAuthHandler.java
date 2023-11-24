// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.login;

import android.app.Activity;
import android.view.WindowManager;

import org.jni_zero.CalledByNative;
import org.jni_zero.NativeMethods;

import org.chromium.base.Callback;
import org.chromium.base.ThreadUtils;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabHidingType;
import org.chromium.components.browser_ui.http_auth.LoginPrompt;
import org.chromium.ui.base.WindowAndroid;

/**
 * Represents an HTTP authentication request to be handled by the UI.
 * The request can be fulfilled or canceled using setAuth() or cancelAuth().
 * This class also provides strings for building a login form.
 *
 * Note: this class supercedes android.webkit.HttpAuthHandler, but does not
 * extend HttpAuthHandler due to the private access of HttpAuthHandler's
 * constructor.
 */
public class ChromeHttpAuthHandler extends EmptyTabObserver implements LoginPrompt.Observer {
    private static Callback<ChromeHttpAuthHandler> sTestCreationCallback;

    private long mNativeChromeHttpAuthHandler;
    private String mAutofillUsername;
    private String mAutofillPassword;
    private LoginPrompt mLoginPrompt;
    private Tab mTab;

    private ChromeHttpAuthHandler(long nativeChromeHttpAuthHandler) {
        assert nativeChromeHttpAuthHandler != 0;
        mNativeChromeHttpAuthHandler = nativeChromeHttpAuthHandler;
        if (sTestCreationCallback != null) sTestCreationCallback.onResult(this);
    }

    @CalledByNative
    private static ChromeHttpAuthHandler create(long nativeChromeHttpAuthHandler) {
        return new ChromeHttpAuthHandler(nativeChromeHttpAuthHandler);
    }

    /** Set a test callback to be notified of all ChromeHttpAuthHandlers created. */
    public static void setTestCreationCallback(Callback<ChromeHttpAuthHandler> callback) {
        ThreadUtils.assertOnUiThread();
        sTestCreationCallback = callback;
    }

    // ---------------------------------------------
    // HttpAuthHandler methods
    // ---------------------------------------------

    // Note on legacy useHttpAuthUsernamePassword() method:
    // For reference, the existing WebView (when using the chromium stack) returns true here
    // iff this is the first auth challenge attempt for this connection.
    // (see WebUrlLoaderClient::authRequired call to didReceiveAuthenticationChallenge)
    // In ChromeHttpAuthHandler this mechanism is superseded by the
    // LoginPrompt.onAutofillDataAvailable mechanism below, however the legacy WebView
    // implementation will need to handle the API mismatch between the legacy
    // WebView.getHttpAuthUsernamePassword synchronous call and the credentials arriving
    // asynchronously in onAutofillDataAvailable.

    @Override
    public void cancel() {
        ChromeHttpAuthHandlerJni.get()
                .cancelAuth(mNativeChromeHttpAuthHandler, ChromeHttpAuthHandler.this);
    }

    @Override
    public void proceed(String username, String password) {
        ChromeHttpAuthHandlerJni.get()
                .setAuth(
                        mNativeChromeHttpAuthHandler,
                        ChromeHttpAuthHandler.this,
                        username,
                        password);
    }

    /** Return whether the auth dialog is being shown. */
    public boolean isShowingAuthDialog() {
        return mLoginPrompt != null && mLoginPrompt.isShowing();
    }

    @CalledByNative
    private void showDialog(Tab tab, WindowAndroid windowAndroid) {
        if (tab == null || tab.isHidden() || windowAndroid == null) {
            cancel();
            return;
        }
        Activity activity = windowAndroid.getActivity().get();
        if (activity == null) {
            cancel();
            return;
        }
        if (activity.isFinishing() || activity.isDestroyed()) {
            cancel();
            return;
        }
        mTab = tab;
        mTab.addObserver(this);
        String messageBody =
                ChromeHttpAuthHandlerJni.get()
                        .getMessageBody(mNativeChromeHttpAuthHandler, ChromeHttpAuthHandler.this);
        mLoginPrompt = new LoginPrompt(activity, messageBody, null, this);
        // In case the autofill data arrives before the prompt is created.
        if (mAutofillUsername != null && mAutofillPassword != null) {
            mLoginPrompt.onAutofillDataAvailable(mAutofillUsername, mAutofillPassword);
        }
        try {
            mLoginPrompt.show();
        } catch (WindowManager.BadTokenException ex) {
            cancel();
            return;
        }
    }

    @CalledByNative
    private void closeDialog() {
        if (mLoginPrompt != null) mLoginPrompt.dismiss();
    }

    @CalledByNative
    private void onNativeDestroyed() {
        mNativeChromeHttpAuthHandler = 0;
        if (mTab != null) mTab.removeObserver(this);
        mTab = null;
    }

    @Override
    public void onHidden(Tab tab, @TabHidingType int reason) {
        cancel();
    }

    @CalledByNative
    private void onAutofillDataAvailable(String username, String password) {
        mAutofillUsername = username;
        mAutofillPassword = password;
        if (mLoginPrompt != null) {
            mLoginPrompt.onAutofillDataAvailable(username, password);
        }
    }

    @NativeMethods
    interface Natives {
        void setAuth(
                long nativeChromeHttpAuthHandler,
                ChromeHttpAuthHandler caller,
                String username,
                String password);

        void cancelAuth(long nativeChromeHttpAuthHandler, ChromeHttpAuthHandler caller);

        String getMessageBody(long nativeChromeHttpAuthHandler, ChromeHttpAuthHandler caller);
    }
}
