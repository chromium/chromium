// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.robolectric;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;

import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.android_webview.GoogleAccountsEvent;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Feature;
import org.chromium.components.signin.GAIAServiceType;

/** Tests for {@link GoogleAccountsEvent}. */
@RunWith(BaseRobolectricTestRunner.class)
public class GoogleAccountsEventTest {
    @Test
    @Feature({"AndroidWebView"})
    public void testGetters() {
        GoogleAccountsEvent event =
                new GoogleAccountsEvent(
                        GAIAServiceType.GAIA_SERVICE_TYPE_ADDSESSION,
                        "https://example.com/continue",
                        true,
                        true);

        assertEquals(GAIAServiceType.GAIA_SERVICE_TYPE_ADDSESSION, event.getServiceType());
        assertEquals("https://example.com/continue", event.getContinueUrl());
        assertTrue(event.isOutermostMainFrame());
        assertTrue(event.isNavigationRequest());
    }
}
