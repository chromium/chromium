// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.dragdrop;

import android.graphics.PointF;

import org.chromium.chrome.browser.multiwindow.MultiWindowUtils;
import org.chromium.chrome.browser.tab.Tab;

/** Drag-Drop objects to be shared across instances. */
public final class DragDropGlobalState {

    private static DragDropGlobalState sInstance = new DragDropGlobalState();

    public int dragSourceInstanceId = MultiWindowUtils.INVALID_INSTANCE_ID;
    public Tab tabBeingDragged;
    public boolean acceptNextDrop;
    public PointF dropLocation;

    public static DragDropGlobalState getInstance() {
        return sInstance;
    }

    public void reset() {
        dragSourceInstanceId = MultiWindowUtils.INVALID_INSTANCE_ID;
        tabBeingDragged = null;
        acceptNextDrop = false;
        dropLocation = null;
    }
}
