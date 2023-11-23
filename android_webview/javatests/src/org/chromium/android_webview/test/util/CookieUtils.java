// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test.util;

import android.app.Instrumentation;

import org.junit.Assert;

import org.chromium.android_webview.AwCookieManager;
import org.chromium.base.Callback;
import org.chromium.base.test.util.CallbackHelper;

/** Useful functions for testing the CookieManager. */
public class CookieUtils {
    private CookieUtils() {}

    /**
     * A CallbackHelper for use with setCookie/removeXXXCookie.
     *
     * @param <T> the callback's parameter type.
     */
    public static class TestCallback<T> implements Callback<T> {
        /**
         * We only have one intresting method on Callback: onResult.
         *
         * @param <T> the callback's parameter type.
         */
        public static class OnResultHelper<T> extends CallbackHelper {
            private T mValue;

            public T getValue() {
                assert getCallCount() > 0;
                return mValue;
            }

            public void notifyCalled(T value) {
                mValue = value;
                notifyCalled();
            }
        }

        private OnResultHelper<T> mOnResultHelper;

        public TestCallback() {
            mOnResultHelper = new OnResultHelper<T>();
        }

        public OnResultHelper getOnResultHelper() {
            return mOnResultHelper;
        }

        @Override
        public void onResult(T value) {
            mOnResultHelper.notifyCalled(value);
        }

        public T getValue() {
            return mOnResultHelper.getValue();
        }
    }

    /**
     * Clear all cookies from the CookieManager synchronously then assert they are gone.
     * @param  cookieManager the CookieManager on which to remove cookies.
     */
    public static void clearCookies(Instrumentation instr, final AwCookieManager cookieManager)
            throws Throwable {
        final TestCallback<Boolean> callback = new TestCallback<Boolean>();
        int callCount = callback.getOnResultHelper().getCallCount();

        instr.runOnMainSync(() -> cookieManager.removeAllCookies(callback));
        callback.getOnResultHelper().waitForCallback(callCount);
        Assert.assertFalse(cookieManager.hasCookies());
    }
}
