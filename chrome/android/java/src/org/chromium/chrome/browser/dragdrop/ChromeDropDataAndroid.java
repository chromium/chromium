// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.dragdrop;

import android.content.Context;

import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.dragdrop.DropDataAndroid;

/** Chrome-specific drop data. */
@NullMarked
public abstract class ChromeDropDataAndroid extends DropDataAndroid {
    public final boolean allowDragToCreateInstance;
    public final int windowId;

    /** Not generated from java */
    ChromeDropDataAndroid(Builder builder) {
        super(null, null, null, null, null);
        allowDragToCreateInstance = builder.mAllowDragToCreateInstance;
        windowId = builder.mWindowId;
    }

    /** Returns true if the associated browser content is Incognito. */
    public abstract boolean isIncognito();

    /** Build clip data text with tab info. */
    public abstract String buildTabClipDataText(Context context);

    /** Get supported MimeTypes for the associated browser content. */
    public abstract String[] getSupportedMimeTypes();

    /** Builder for @{@link ChromeDropDataAndroid} instance. */
    public abstract static class Builder {
        private boolean mAllowDragToCreateInstance;
        private int mWindowId;

        /**
         * @param allowDragToCreateInstance Whether tab drag to create new instance should be
         *     allowed.
         * @return {@link ChromeDropDataAndroid.Builder} instance.
         */
        public Builder withAllowDragToCreateInstance(boolean allowDragToCreateInstance) {
            mAllowDragToCreateInstance = allowDragToCreateInstance;
            return this;
        }

        /**
         * @param windowId The ID of the window where drag starts.
         * @return {@link ChromeDropDataAndroid.Builder} instance.
         */
        public Builder withWindowId(int windowId) {
            mWindowId = windowId;
            return this;
        }

        /**
         * @return new @{@link ChromeDropDataAndroid} instance.
         */
        public abstract ChromeDropDataAndroid build();
    }
}
