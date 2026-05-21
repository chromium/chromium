// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp_customization;

import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.when;

import android.view.View;
import android.view.Window;

import org.chromium.base.UnownedUserDataHost;
import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.edge_to_edge.EdgeToEdgeStateProvider;

/** Helper class for NTP customization unit tests. */
@NullMarked
public class NtpCustomizationTestHelper {
    /**
     * Set up the Edge-to-Edge environment for the given WindowAndroid.
     *
     * @param windowAndroid The WindowAndroid to set up.
     * @return The created EdgeToEdgeStateProvider.
     */
    public static EdgeToEdgeStateProvider setupEdgeToEdge(WindowAndroid windowAndroid) {
        UnownedUserDataHost windowUserDataHost = new UnownedUserDataHost();
        when(windowAndroid.getUnownedUserDataHost()).thenReturn(windowUserDataHost);

        Window window = mock(Window.class);
        View decorView = mock(View.class);
        when(window.getDecorView()).thenReturn(decorView);

        EdgeToEdgeStateProvider edgeToEdgeStateProvider = new EdgeToEdgeStateProvider(window);
        edgeToEdgeStateProvider.attach(windowAndroid);
        edgeToEdgeStateProvider.acquireSetDecorFitsSystemWindowToken();
        return edgeToEdgeStateProvider;
    }
}
