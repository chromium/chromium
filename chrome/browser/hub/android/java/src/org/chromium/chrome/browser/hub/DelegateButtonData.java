// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.hub;

import android.content.Context;
import android.graphics.drawable.Drawable;

/**
 * Holds a {@link Runnable} for handling on press, and delegates everything else to a {@link
 * DisplayButtonData}.
 */
public class DelegateButtonData implements FullButtonData {
    private final DisplayButtonData mDelegateButtonData;
    private final Runnable mOnPress;

    /** Stores parameters until resolution time. Never invokes {@link Runnable} itself. */
    public DelegateButtonData(DisplayButtonData delegateButtonData, Runnable onPress) {
        mDelegateButtonData = delegateButtonData;
        mOnPress = onPress;
    }

    @Override
    public String resolveText(Context context) {
        return mDelegateButtonData.resolveText(context);
    }

    @Override
    public String resolveContentDescription(Context context) {
        return mDelegateButtonData.resolveContentDescription(context);
    }

    @Override
    public Drawable resolveIcon(Context context) {
        return mDelegateButtonData.resolveIcon(context);
    }

    @Override
    public Runnable getOnPressRunnable() {
        return mOnPress;
    }
}
