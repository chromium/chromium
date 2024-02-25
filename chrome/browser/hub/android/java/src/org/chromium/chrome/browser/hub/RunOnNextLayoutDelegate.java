// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.hub;

import android.view.View;

import org.chromium.base.ThreadUtils.ThreadChecker;

import java.util.ArrayList;
import java.util.List;

/**
 * A delegate to implementation for {@link View}s that want to expose the ability for a runnable to
 * be executed on the next layout.
 */
public class RunOnNextLayoutDelegate implements RunOnNextLayout {
    private final ThreadChecker mThreadChecker = new ThreadChecker();
    private final View mView;
    private List<Runnable> mRunnables = new ArrayList<>();

    /**
     * Constructor for offering {@link RunOnNextLayout} functionality to a view.
     *
     * @param view The {@link View} to add the functionality to.
     */
    public RunOnNextLayoutDelegate(View view) {
        mView = view;
    }

    @Override
    public void runOnNextLayout(Runnable runnable) {
        mThreadChecker.assertOnValidThread();

        mRunnables.add(runnable);
        boolean isLayoutNotRequested = !mView.isLayoutRequested();
        if (isLayoutNotRequested && isLaidOut()) {
            // Already laid out, run immediately.
            runOnNextLayoutRunnables();
        } else if (isLayoutNotRequested) {
            // Post the runnable to delay in case a layout happens.
            mView.post(this::runOnNextLayoutRunnables);
        }
    }

    @Override
    public void runOnNextLayoutRunnables() {
        mThreadChecker.assertOnValidThread();

        // Make this re-entrancy safe.
        List<Runnable> runnables = mRunnables;
        mRunnables = new ArrayList<Runnable>();

        for (Runnable runnable : runnables) {
            runnable.run();
        }
    }

    private boolean isLaidOut() {
        return mView.getHeight() > 0 && mView.getWidth() > 0 && mView.isLaidOut();
    }
}
