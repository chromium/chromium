// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.access_loss;

import static org.junit.Assert.assertEquals;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.verify;

import static org.chromium.chrome.browser.access_loss.HelpUrlLauncher.KEEP_APPS_AND_DEVICES_WORKING_WITH_GMS_CORE_SUPPORT_URL;

import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.net.Uri;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.password_manager.CustomTabIntentHelper;

/** Unit tests for {@link HelpUrlLauncher}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class HelpUrlLauncherTest {
    private Activity mActivity;

    private HelpUrlLauncher mHelpUrlLauncher;

    private CustomTabIntentHelper mCustomTabIntentHelper;

    @Before
    public void setUp() {
        mActivity = Robolectric.buildActivity(Activity.class).create().start().resume().get();
        mActivity.setTheme(R.style.Theme_BrowserUI_DayNight);
        mCustomTabIntentHelper = (Context context, Intent intent) -> intent;
        mHelpUrlLauncher = new HelpUrlLauncher(mCustomTabIntentHelper);
    }

    @Test
    public void lauchesHelpUrl() {
        Activity spyActivity = spy(mActivity);
        mHelpUrlLauncher.showHelpArticle(
                spyActivity, KEEP_APPS_AND_DEVICES_WORKING_WITH_GMS_CORE_SUPPORT_URL);
        ArgumentCaptor<Intent> intentArgumentCaptor = ArgumentCaptor.forClass(Intent.class);
        verify(spyActivity).startActivity(intentArgumentCaptor.capture(), any());
        assertEquals(
                Uri.parse(KEEP_APPS_AND_DEVICES_WORKING_WITH_GMS_CORE_SUPPORT_URL),
                intentArgumentCaptor.getValue().getData());
    }
}
