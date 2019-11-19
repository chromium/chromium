// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.init;

import android.view.View;
import android.view.ViewTreeObserver;

import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.content_public.browser.UiThreadTaskTraits;

/**
 * A utility for observing when a view gets drawn for the first time.
 */
class FirstDrawDetector {
    View mView;
    Runnable mCallback;

    private FirstDrawDetector(View view, Runnable callback) {
        mView = view;
        mCallback = callback;
    }

    /**
     * Waits for a view to be drawn on the screen for the first time.
     * @param view View whose drawing to observe.
     * @param callback Callback to trigger on first draw. Will be called on the UI thread.
     */
    public static void waitForFirstDraw(View view, Runnable callback) {
        new FirstDrawDetector(view, callback).startWaiting();
    }

    private void startWaiting() {
        // We use a draw listener to detect when a view is first drawn. However, if the view
        // doesn't get drawn for some reason (e.g. the screen is off), our listener will never
        // get called. To work around this, we also schedule a callback for the next frame from
        // a pre-draw listener (which will always get called). Whichever callback runs first
        // will declare the view to have been drawn.
        //
        // Note that we cannot just use a pre-draw listener here, because it does not guarantee
        // that the view has actually been drawn.
        ViewTreeObserver.OnPreDrawListener firstPreDrawListener =
                new ViewTreeObserver.OnPreDrawListener() {
                    @Override
                    public boolean onPreDraw() {
                        // The pre-draw listener will run both when the screen is on or off, but the
                        // view might not have been drawn yet at this point. Trigger the first paint
                        // at the next frame.
                        PostTask.postTask(TaskTraits.CHOREOGRAPHER_FRAME, () -> onFirstDraw());
                        mView.getViewTreeObserver().removeOnPreDrawListener(this);
                        return true;
                    }
                };
        ViewTreeObserver.OnDrawListener firstDrawListener = new ViewTreeObserver.OnDrawListener() {
            @Override
            public void onDraw() {
                // This callback will be run in the normal case (e.g., screen is on).
                onFirstDraw();
                // The draw listener can't be removed from within the callback, so remove it
                // asynchronously.
                PostTask.postTask(UiThreadTaskTraits.BEST_EFFORT,
                        () -> mView.getViewTreeObserver().removeOnDrawListener(this));
            }
        };
        mView.getViewTreeObserver().addOnPreDrawListener(firstPreDrawListener);
        mView.getViewTreeObserver().addOnDrawListener(firstDrawListener);
    }

    private void onFirstDraw() {
        if (mCallback != null) {
            mCallback.run();
            mCallback = null;
        }
    }
}
