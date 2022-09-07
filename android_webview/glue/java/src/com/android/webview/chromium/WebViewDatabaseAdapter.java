// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.android.webview.chromium;

import android.os.Build;
import android.webkit.WebViewDatabase;

import org.chromium.android_webview.AwFormDatabase;
import org.chromium.android_webview.HttpAuthDatabase;
import org.chromium.base.ThreadUtils;

import java.util.concurrent.Callable;

/**
 * Chromium implementation of WebViewDatabase -- forwards calls to the
 * chromium internal implementation.
 */
@SuppressWarnings("deprecation")
final class WebViewDatabaseAdapter extends WebViewDatabase {
    private final WebViewChromiumFactoryProvider mFactory;
    private final HttpAuthDatabase mHttpAuthDatabase;

    public WebViewDatabaseAdapter(
            WebViewChromiumFactoryProvider factory, HttpAuthDatabase httpAuthDatabase) {
        mFactory = factory;
        mHttpAuthDatabase = httpAuthDatabase;
    }

    @Override
    public boolean hasUsernamePassword() {
        // This is a deprecated API: intentional no-op.
        return false;
    }

    @Override
    public void clearUsernamePassword() {
        // This is a deprecated API: intentional no-op.
    }

    @Override
    public boolean hasHttpAuthUsernamePassword() {
        if (checkNeedsPost()) {
            return mFactory.runOnUiThreadBlocking(new Callable<Boolean>() {
                @Override
                public Boolean call() {
                    return mHttpAuthDatabase.hasHttpAuthUsernamePassword();
                }

            });
        }
        return mHttpAuthDatabase.hasHttpAuthUsernamePassword();
    }

    @Override
    public void clearHttpAuthUsernamePassword() {
        if (checkNeedsPost()) {
            mFactory.addTask(new Runnable() {
                @Override
                public void run() {
                    mHttpAuthDatabase.clearHttpAuthUsernamePassword();
                }

            });
            return;
        }
        mHttpAuthDatabase.clearHttpAuthUsernamePassword();
    }

    @Override
    public void setHttpAuthUsernamePassword(
            final String host, final String realm, final String username, final String password) {
        if (checkNeedsPost()) {
            mFactory.addTask(new Runnable() {
                @Override
                public void run() {
                    mHttpAuthDatabase.setHttpAuthUsernamePassword(host, realm, username, password);
                }
            });
            return;
        }
        mHttpAuthDatabase.setHttpAuthUsernamePassword(host, realm, username, password);
    }

    @Override
    public String[] getHttpAuthUsernamePassword(final String host, final String realm) {
        if (checkNeedsPost()) {
            return mFactory.runOnUiThreadBlocking(new Callable<String[]>() {
                @Override
                public String[] call() {
                    return mHttpAuthDatabase.getHttpAuthUsernamePassword(host, realm);
                }
            });
        }
        return mHttpAuthDatabase.getHttpAuthUsernamePassword(host, realm);
    }

    @Override
    public boolean hasFormData() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) return false;

        if (checkNeedsPost()) {
            return mFactory.runOnUiThreadBlocking(new Callable<Boolean>() {
                @Override
                public Boolean call() {
                    return AwFormDatabase.hasFormData();
                }

            });
        }
        return AwFormDatabase.hasFormData();
    }

    @Override
    public void clearFormData() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) return;

        if (checkNeedsPost()) {
            mFactory.addTask(new Runnable() {
                @Override
                public void run() {
                    AwFormDatabase.clearFormData();
                }

            });
            return;
        }
        AwFormDatabase.clearFormData();
    }

    private static boolean checkNeedsPost() {
        // Init is guaranteed to have happened if a WebViewDatabaseAdapter is created, so do not
        // need to check WebViewChromiumFactoryProvider.hasStarted.
        return !ThreadUtils.runningOnUiThread();
    }
}
