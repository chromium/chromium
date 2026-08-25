// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.accessibility;

import org.chromium.android_webview.AppState;
import org.chromium.android_webview.AwContentsLifecycleNotifier;
import org.chromium.base.ThreadUtils;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.ui.accessibility.AccessibilityStateVisibilityManager;

/** Manages webview accessibility state visibility. */
@NullMarked
public class AwAccessibilityStateVisibilityManager
        implements AccessibilityStateVisibilityManager, AwContentsLifecycleNotifier.Observer {
    private AccessibilityStateVisibilityManager.@Nullable Observer mObserver;

    @Override
    public void setObserver(@Nullable Observer observer) {
        assert ThreadUtils.runningOnUiThread();
        if (mObserver != null) {
            AwContentsLifecycleNotifier.getInstance().removeObserver(this);
        }
        mObserver = observer;
        if (mObserver != null) {
            AwContentsLifecycleNotifier.getInstance().addObserver(this);
        }
    }

    @Override
    public void onFirstWebViewCreated() {}

    @Override
    public void onLastWebViewDestroyed() {}

    @Override
    public void onAppStateChanged(@AppState int appState) {
        if (mObserver == null) {
            return;
        }

        if (appState == AppState.FOREGROUND) {
            mObserver.onApplicationForegrounded();
            mObserver.onAnyActivityMadeVisible();
        } else {
            mObserver.onApplicationBackgrounded();
        }
    }
}
