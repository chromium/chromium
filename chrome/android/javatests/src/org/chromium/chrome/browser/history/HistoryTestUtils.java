// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.history;

import android.os.Handler;
import android.os.Looper;
import android.support.v7.widget.RecyclerView;

import org.junit.Assert;

import org.chromium.base.test.util.CallbackHelper;
import org.chromium.chrome.browser.preferences.PrefChangeRegistrar.PrefObserver;
import org.chromium.chrome.browser.signin.SigninManager.SignInStateObserver;
import org.chromium.chrome.browser.widget.DateDividedAdapter.ItemViewType;
import org.chromium.chrome.browser.widget.selection.SelectionDelegate.SelectionObserver;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

import java.util.List;

/**
 * Util class for functions and helper classes that share between different test files.
 */
public class HistoryTestUtils {
    /**
     * Test Observer that used to collect the callback counts used in {@link HistoryActivityTest}
     * {@link HistoryActivityScrollingTest}.
     */
    static class TestObserver extends RecyclerView.AdapterDataObserver
            implements SelectionObserver<HistoryItem>, SignInStateObserver, PrefObserver {
        public final CallbackHelper onChangedCallback = new CallbackHelper();
        public final CallbackHelper onSelectionCallback = new CallbackHelper();
        public final CallbackHelper onSigninStateChangedCallback = new CallbackHelper();
        public final CallbackHelper onPreferenceChangeCallback = new CallbackHelper();

        private Handler mHandler;

        public TestObserver() {
            mHandler = new Handler(Looper.getMainLooper());
        }

        @Override
        public void onChanged() {
            // To guarantee that all real Observers have had a chance to react to the event, post
            // the CallbackHelper.notifyCalled() call.
            mHandler.post(() -> onChangedCallback.notifyCalled());
        }

        @Override
        public void onSelectionStateChange(List<HistoryItem> selectedItems) {
            mHandler.post(() -> onSelectionCallback.notifyCalled());
        }

        @Override
        public void onSignedIn() {
            mHandler.post(() -> onSigninStateChangedCallback.notifyCalled());
        }

        @Override
        public void onSignedOut() {
            mHandler.post(() -> onSigninStateChangedCallback.notifyCalled());
        }

        @Override
        public void onPreferenceChange() {
            mHandler.post(() -> onPreferenceChangeCallback.notifyCalled());
        }
    }

    static void setupHistoryTestHeaders(HistoryAdapter adapter, TestObserver observer)
            throws Exception {
        if (!adapter.isClearBrowsingDataButtonVisible()) {
            int changedCallCount = observer.onChangedCallback.getCallCount();
            TestThreadUtils.runOnUiThreadBlocking(
                    () -> adapter.setClearBrowsingDataButtonVisibilityForTest(true));
            observer.onChangedCallback.waitForCallback(changedCallCount);
        }

        if (adapter.arePrivacyDisclaimersVisible()) {
            int changedCallCount = observer.onChangedCallback.getCallCount();
            TestThreadUtils.runOnUiThreadBlocking(() -> adapter.hasOtherFormsOfBrowsingData(false));
            observer.onChangedCallback.waitForCallback(changedCallCount);
        }
    }

    static void checkAdapterContents(
            HistoryAdapter adapter, boolean hasHeader, boolean hasFooter, Object... items) {
        Assert.assertEquals(items.length, adapter.getItemCount());
        Assert.assertEquals(hasHeader, adapter.hasListHeader());
        Assert.assertEquals(hasFooter, adapter.hasListFooter());

        for (int i = 0; i < items.length; i++) {
            if (i == 0 && hasHeader) {
                Assert.assertEquals(ItemViewType.HEADER, adapter.getItemViewType(i));
                continue;
            }

            if (hasFooter && i == items.length - 1) {
                Assert.assertEquals(ItemViewType.FOOTER, adapter.getItemViewType(i));
                continue;
            }

            if (items[i] == null) {
                // TODO(twellington): Check what date header is showing.
                Assert.assertEquals(ItemViewType.DATE, adapter.getItemViewType(i));
            } else {
                Assert.assertEquals(ItemViewType.NORMAL, adapter.getItemViewType(i));
                Assert.assertEquals(items[i], adapter.getItemAt(i).second);
            }
        }
    }
}
