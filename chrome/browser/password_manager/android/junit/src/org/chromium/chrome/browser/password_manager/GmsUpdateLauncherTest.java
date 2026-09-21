// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_manager;

import static org.junit.Assert.assertEquals;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.content.Context;
import android.content.Intent;
import android.net.Uri;

import com.google.android.gms.common.GoogleApiAvailability;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;

@RunWith(BaseRobolectricTestRunner.class)
public class GmsUpdateLauncherTest {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Mock private Context mContext;
    @Captor private ArgumentCaptor<Intent> mIntentCaptor;

    @Test
    public void testSendsIntentOnLaunchGmsUpdate() {
        when(mContext.getPackageName()).thenReturn("org.chromium.chrome");

        GmsUpdateLauncher.launch(mContext);
        verify(mContext).startActivity(mIntentCaptor.capture());
        Intent intent = mIntentCaptor.getValue();

        assertEquals(Intent.ACTION_VIEW, intent.getAction());
        assertEquals("com.android.vending", intent.getPackage());
        assertEquals("org.chromium.chrome", intent.getStringExtra("callerId"));
        assertEquals(Intent.FLAG_ACTIVITY_NEW_TASK, intent.getFlags());
        assertEquals(
                intent.getData(),
                Uri.parse(
                        "market://details?id="
                                + GoogleApiAvailability.GOOGLE_PLAY_SERVICES_PACKAGE
                                + "&referrer=chrome_upm"));
    }
}
