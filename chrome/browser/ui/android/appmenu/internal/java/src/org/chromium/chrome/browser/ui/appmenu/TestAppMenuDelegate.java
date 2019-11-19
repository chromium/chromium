// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.appmenu;

import android.os.Bundle;

import androidx.annotation.Nullable;

import org.chromium.base.test.util.CallbackHelper;

class TestAppMenuDelegate implements AppMenuDelegate {
    public final CallbackHelper itemSelectedCallbackHelper = new CallbackHelper();
    public int lastSelectedItemId;

    @Override
    public boolean onOptionsItemSelected(int itemId, @Nullable Bundle menuItemData) {
        lastSelectedItemId = itemId;
        itemSelectedCallbackHelper.notifyCalled();
        return true;
    }

    @Override
    public AppMenuPropertiesDelegate createAppMenuPropertiesDelegate() {
        return new TestAppMenuPropertiesDelegate();
    }
}
