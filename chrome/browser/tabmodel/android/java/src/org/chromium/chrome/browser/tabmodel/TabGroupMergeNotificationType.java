// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import androidx.annotation.IntDef;

import org.chromium.build.annotations.NullMarked;

import java.lang.annotation.ElementType;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.lang.annotation.Target;

/** Whether and how to notify observers about tab group merge events. */
@NullMarked
@IntDef({
    TabGroupMergeNotificationType.DONT_NOTIFY,
    TabGroupMergeNotificationType.NOTIFY_IF_NOT_NEW_GROUP,
    TabGroupMergeNotificationType.NOTIFY_ALWAYS
})
@Target(ElementType.TYPE_USE)
@Retention(RetentionPolicy.SOURCE)
public @interface TabGroupMergeNotificationType {
    int DONT_NOTIFY = 0;
    int NOTIFY_IF_NOT_NEW_GROUP = 1;
    int NOTIFY_ALWAYS = 2;
}
