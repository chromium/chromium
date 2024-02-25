// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.media.ui;

import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.mock;

import android.content.res.Resources;
import android.graphics.Color;
import android.graphics.drawable.ColorDrawable;
import android.graphics.drawable.Drawable;

import org.robolectric.annotation.Implementation;
import org.robolectric.annotation.Implements;
import org.robolectric.shadows.ShadowResources;

/** Dummy Robolectric shadow for Android Resources for MediaNotification tests. */
@Implements(Resources.class)
public class MediaNotificationTestShadowResources extends ShadowResources {
    public static final Resources sResources;

    static {
        sResources = mock(Resources.class);
        doReturn("mocked text").when(sResources).getText(anyInt());
        doReturn("mocked resource name").when(sResources).getResourceName(anyInt());
    }

    @Implementation
    public CharSequence getText(int id) {
        return sResources.getText(id);
    }

    @Implementation
    public CharSequence getResourceName(int id) {
        return sResources.getResourceName(id);
    }

    @Implementation
    public Drawable getDrawable(int id, Resources.Theme theme) {
        return new ColorDrawable(Color.RED);
    }
}
