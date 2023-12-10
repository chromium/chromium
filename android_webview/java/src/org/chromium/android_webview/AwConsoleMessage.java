// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview;

import androidx.annotation.IntDef;

import org.chromium.android_webview.common.Lifetime;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 *
 * See {@link android.webkit.ConsoleMessage}. */
@Lifetime.Temporary
public class AwConsoleMessage {
    @Retention(RetentionPolicy.SOURCE)
    @IntDef({
        MESSAGE_LEVEL_TIP,
        MESSAGE_LEVEL_LOG,
        MESSAGE_LEVEL_WARNING,
        MESSAGE_LEVEL_ERROR,
        MESSAGE_LEVEL_DEBUG
    })
    public @interface MessageLevel {}

    public static final int MESSAGE_LEVEL_TIP = 0;
    public static final int MESSAGE_LEVEL_LOG = 1;
    public static final int MESSAGE_LEVEL_WARNING = 2;
    public static final int MESSAGE_LEVEL_ERROR = 3;
    public static final int MESSAGE_LEVEL_DEBUG = 4;

    @MessageLevel private int mLevel;
    private String mMessage;
    private String mSourceId;
    private int mLineNumber;

    public AwConsoleMessage(
            String message, String sourceId, int lineNumber, @MessageLevel int msgLevel) {
        mMessage = message;
        mSourceId = sourceId;
        mLineNumber = lineNumber;
        mLevel = msgLevel;
    }

    @MessageLevel
    public int messageLevel() {
        return mLevel;
    }

    public String message() {
        return mMessage;
    }

    public String sourceId() {
        return mSourceId;
    }

    public int lineNumber() {
        return mLineNumber;
    }
}
