// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_manager;

import static org.junit.Assert.assertEquals;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.verify;

import android.content.Context;
import android.content.Intent;
import android.net.Uri;

import com.google.android.gms.common.GoogleApiAvailability;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;

@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class GmsUpdateLauncherTest {
    @Test
    public void testSendsIntentOnLaunchGmsUpdate() {
        Context mockContext = mock(Context.class);

        GmsUpdateLauncher.launch(mockContext);
        ArgumentCaptor<Intent> intentArgumentCaptor = ArgumentCaptor.forClass(Intent.class);
        verify(mockContext).startActivity(intentArgumentCaptor.capture());
        Intent intent = intentArgumentCaptor.getValue();

        assertEquals(intent.getAction(), Intent.ACTION_VIEW);
        assertEquals(intent.getPackage(), "com.android.vending");
        assertEquals(intent.getStringExtra("callerId"), mockContext.getPackageName());
        assertEquals(intent.getFlags(), Intent.FLAG_ACTIVITY_NEW_TASK);
        assertEquals(
                intent.getData(),
                Uri.parse(
                        "market://details?id="
                                + GoogleApiAvailability.GOOGLE_PLAY_SERVICES_PACKAGE
                                + "&referrer=chrome_upm"));
    }
}
