// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.multiwindow;

import android.app.Activity;
import android.content.Intent;
import android.os.Bundle;

import org.chromium.base.ObserverList;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;

/** Implementation of {@link MultiWindowModeStateDispatcher}. */
public class MultiWindowModeStateDispatcherImpl implements MultiWindowModeStateDispatcher {
    private final Activity mActivity;
    private final ObserverList<MultiWindowModeObserver> mObservers;

    /**
     * Construct a new MultiWindowModeStateDispatcher.
     * @param activity The activity associated with this state dispatcher.
     */
    public MultiWindowModeStateDispatcherImpl(Activity activity) {
        mActivity = activity;
        mObservers = new ObserverList<>();
    }

    /**
     * Notify observers that multi-window mode has changed.
     * @param inMultiWindowMode Whether the activity is currently in multi-window mode.
     */
    public void dispatchMultiWindowModeChanged(boolean inMultiWindowMode) {
        for (MultiWindowModeObserver observer : mObservers) {
            observer.onMultiWindowModeChanged(inMultiWindowMode);
        }
    }

    @Override
    public boolean addObserver(MultiWindowModeObserver observer) {
        return mObservers.addObserver(observer);
    }

    @Override
    public boolean removeObserver(MultiWindowModeObserver observer) {
        return mObservers.removeObserver(observer);
    }

    @Override
    public boolean isInMultiWindowMode() {
        return MultiWindowUtils.getInstance().isInMultiWindowMode(mActivity);
    }

    @Override
    public boolean isInMultiDisplayMode() {
        return MultiWindowUtils.getInstance().isInMultiDisplayMode(mActivity);
    }

    @Override
    public boolean isMultiInstanceRunning() {
        return MultiWindowUtils.getInstance().areMultipleChromeInstancesRunning(mActivity);
    }

    @Override
    public boolean isChromeRunningInAdjacentWindow() {
        return MultiWindowUtils.getInstance().isChromeRunningInAdjacentWindow(mActivity);
    }

    @Override
    public boolean isOpenInOtherWindowSupported() {
        return MultiWindowUtils.getInstance().isOpenInOtherWindowSupported(mActivity);
    }

    @Override
    public boolean isMoveToOtherWindowSupported(TabModelSelector tabModelSelector) {
        return MultiWindowUtils.getInstance()
                .isMoveToOtherWindowSupported(mActivity, tabModelSelector);
    }

    @Override
    public boolean canEnterMultiWindowMode() {
        return MultiWindowUtils.getInstance().canEnterMultiWindowMode(mActivity);
    }

    @Override
    public Class<? extends Activity> getOpenInOtherWindowActivity() {
        return MultiWindowUtils.getInstance().getOpenInOtherWindowActivity(mActivity);
    }

    @Override
    public Intent getOpenInOtherWindowIntent() {
        Class<? extends Activity> targetActivity = getOpenInOtherWindowActivity();
        if (targetActivity == null) return null;

        Intent intent = new Intent(mActivity, targetActivity);
        MultiWindowUtils.setOpenInOtherWindowIntentExtras(intent, mActivity, targetActivity);

        return intent;
    }

    @Override
    public Bundle getOpenInOtherWindowActivityOptions() {
        return MultiWindowUtils.getOpenInOtherWindowActivityOptions(mActivity);
    }

    @Override
    public int getInstanceCount() {
        return MultiWindowUtils.getInstanceCount();
    }
}
