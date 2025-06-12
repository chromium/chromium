// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share.scroll_capture;

import android.graphics.Rect;
import android.os.Build.VERSION_CODES;
import android.os.CancellationSignal;
import android.view.ScrollCaptureCallback;
import android.view.ScrollCaptureSession;

import androidx.annotation.RequiresApi;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.share.scroll_capture.ScrollCaptureCallbackDelegate.EntryManagerWrapper;
import org.chromium.chrome.browser.tab.Tab;

import java.util.function.Consumer;

/**
 * Implementation of ScrollCaptureCallback that provides snapshots of the tab using Paint Previews.
 * See {@link ScrollCaptureCallbackDelegate} for the bulk of the code.
 */
@NullMarked
@RequiresApi(api = VERSION_CODES.S)
public class ScrollCaptureCallbackImpl implements ScrollCaptureCallback {
    private final ScrollCaptureCallbackDelegate mDelegate;

    public ScrollCaptureCallbackImpl(EntryManagerWrapper entryManagerWrapper) {
        mDelegate = new ScrollCaptureCallbackDelegate(entryManagerWrapper);
    }

    @Override
    // TODO(crbug.com/40779510): work out why this is causing a lint error
    @SuppressWarnings("Override")
    public void onScrollCaptureSearch(
            CancellationSignal cancellationSignal, Consumer<Rect> onReady) {
        Rect r = mDelegate.onScrollCaptureSearch(cancellationSignal);
        onReady.accept(r);
    }

    @Override
    // TODO(crbug.com/40779510): work out why this is causing a lint error
    @SuppressWarnings("Override")
    public void onScrollCaptureStart(
            ScrollCaptureSession session, CancellationSignal signal, Runnable onReady) {
        mDelegate.onScrollCaptureStart(signal, onReady);
    }

    @Override
    // TODO(crbug.com/40779510): work out why this is causing a lint error
    @SuppressWarnings("Override")
    public void onScrollCaptureImageRequest(
            ScrollCaptureSession session,
            CancellationSignal signal,
            Rect captureArea,
            Consumer<Rect> onComplete) {
        mDelegate.onScrollCaptureImageRequest(
                session.getSurface(),
                signal,
                captureArea,
                (r) -> {
                    onComplete.accept(r);
                });
    }

    @Override
    // TODO(crbug.com/40779510): work out why this is causing a lint error
    @SuppressWarnings("Override")
    public void onScrollCaptureEnd(Runnable onReady) {
        mDelegate.onScrollCaptureEnd(onReady);
    }

    void setCurrentTab(Tab tab) {
        mDelegate.setCurrentTab(tab);
    }
}
