// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.firstrun;

import org.chromium.base.test.util.CallbackHelper;

import java.util.HashMap;
import java.util.Map;

/** Records when the FirstRunActivity has progressed through various states. */
public class FirstRunActivityTestObserver implements FirstRunActivity.FirstRunActivityObserver {
    /** Holds data and callbacks from observer methods that originated from a specific caller. */
    public static class ScopedObserverData {
        public final CallbackHelper createPostNativeAndPoliciesPageSequenceCallback =
                new CallbackHelper();
        public final CallbackHelper acceptTermsOfServiceCallback = new CallbackHelper();
        public final CallbackHelper jumpToPageCallback = new CallbackHelper();
        public final CallbackHelper updateCachedEngineCallback = new CallbackHelper();
        public final CallbackHelper abortFirstRunExperienceCallback = new CallbackHelper();
        public final CallbackHelper exitFirstRunCallback = new CallbackHelper();
    }

    private final Map<FirstRunActivity, ScopedObserverData> mScopeObserverDataMap = new HashMap<>();

    public ScopedObserverData getScopedObserverData(FirstRunActivity firstRunActivity) {
        if (!mScopeObserverDataMap.containsKey(firstRunActivity)) {
            mScopeObserverDataMap.put(firstRunActivity, new ScopedObserverData());
        }
        return mScopeObserverDataMap.get(firstRunActivity);
    }

    @Override
    public void onCreatePostNativeAndPoliciesPageSequence(FirstRunActivity caller) {
        getScopedObserverData(caller)
                .createPostNativeAndPoliciesPageSequenceCallback
                .notifyCalled();
    }

    @Override
    public void onAcceptTermsOfService(FirstRunActivity caller) {
        getScopedObserverData(caller).acceptTermsOfServiceCallback.notifyCalled();
    }

    @Override
    public void onJumpToPage(FirstRunActivity caller, int position) {
        getScopedObserverData(caller).jumpToPageCallback.notifyCalled();
    }

    @Override
    public void onUpdateCachedEngineName(FirstRunActivity caller) {
        getScopedObserverData(caller).updateCachedEngineCallback.notifyCalled();
    }

    @Override
    public void onAbortFirstRunExperience(FirstRunActivity caller) {
        getScopedObserverData(caller).abortFirstRunExperienceCallback.notifyCalled();
    }

    @Override
    public void onExitFirstRun(FirstRunActivity caller) {
        getScopedObserverData(caller).exitFirstRunCallback.notifyCalled();
    }
}
