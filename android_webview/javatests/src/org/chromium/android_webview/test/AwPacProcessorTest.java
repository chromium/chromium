// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import static org.chromium.android_webview.test.OnlyRunIn.ProcessMode.EITHER_PROCESS;

import android.os.Build;

import androidx.annotation.RequiresApi;
import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.Parameterized;
import org.junit.runners.Parameterized.UseParametersRunnerFactory;

import org.chromium.android_webview.AwPacProcessor;
import org.chromium.base.library_loader.LibraryLoader;
import org.chromium.base.test.util.MinAndroidSdkLevel;

import java.util.List;

/** Tests for AwPacProcessor class. */
@RunWith(Parameterized.class)
@UseParametersRunnerFactory(AwJUnit4ClassRunnerWithParameters.Factory.class)
@MinAndroidSdkLevel(Build.VERSION_CODES.P)
@RequiresApi(Build.VERSION_CODES.P)
@OnlyRunIn(EITHER_PROCESS) // These tests don't use the renderer process
public class AwPacProcessorTest extends AwParameterizedTest {
    private AwPacProcessor mProcessor;

    private static final String PAC_SCRIPT =
            """
        function FindProxyForURL(url, host) {
          var x = myIpAddress();
          return "PROXY " + x + ":80";
        }
        """;
    private static final String M_TEST_URL = "http://testurl.test";

    @Rule public AwActivityTestRule mRule;

    public AwPacProcessorTest(AwSettingsMutation param) {
        this.mRule = new AwActivityTestRule(param.getMutation());
    }

    @Before
    public void setUp() {
        LibraryLoader.getInstance().ensureInitialized();

        mProcessor = AwPacProcessor.getInstance();
    }

    @Test
    @SmallTest
    public void testUpdateNetworkAndLinkAddresses() throws Throwable {
        // PAC script returns result of myIpAddress call
        mProcessor.setProxyScript(PAC_SCRIPT);

        // Save the proxy request result when network is not set
        String proxyResultNetworkIsNotSet = mProcessor.makeProxyRequest(M_TEST_URL);

        // Set network and IP addresses, check they are correctly propagated.
        mProcessor.setNetworkAndLinkAddresses(42, List.of("1.2.3.4"));
        String proxyResultNetworkIsSet = mProcessor.makeProxyRequest(M_TEST_URL);
        Assert.assertEquals("PROXY 1.2.3.4:80", proxyResultNetworkIsSet);

        // Unset network, the returned proxy string must be equal previously saved value
        mProcessor.setNetwork(null);
        Assert.assertEquals(proxyResultNetworkIsNotSet, mProcessor.makeProxyRequest(M_TEST_URL));
    }
}
