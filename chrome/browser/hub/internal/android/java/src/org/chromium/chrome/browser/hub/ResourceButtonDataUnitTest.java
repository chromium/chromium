// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.hub;

import static org.junit.Assert.assertNotNull;

import android.content.Context;

import androidx.test.core.app.ApplicationProvider;
import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseRobolectricTestRunner;

/** Unit tests for {@link ResourceButtonData}. */
@RunWith(BaseRobolectricTestRunner.class)
public class ResourceButtonDataUnitTest {
    @Test
    @SmallTest
    public void testFocusChangesPane() {
        Context context = ApplicationProvider.getApplicationContext();
        DisplayButtonData buttonData =
                new ResourceButtonData(R.string.button_new_tab, R.drawable.ic_add);
        Assert.assertNotEquals(0, buttonData.resolveText(context).length());
        assertNotNull(buttonData.resolveIcon(context));
    }
}
