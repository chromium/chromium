// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import static org.chromium.android_webview.test.OnlyRunIn.ProcessMode.EITHER_PROCESS;

import android.os.Build;

import androidx.annotation.RequiresApi;
import androidx.test.filters.SmallTest;

import com.android.webview.chromium.WebViewChromiumFactoryProvider;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.Parameterized;
import org.junit.runners.Parameterized.UseParametersRunnerFactory;

import org.chromium.android_webview.AwPacProcessor;
import org.chromium.base.JNIUtils;
import org.chromium.base.library_loader.LibraryLoader;
import org.chromium.base.test.util.MinAndroidSdkLevel;

/** Tests for AwPacProcessor class. */
@RunWith(Parameterized.class)
@UseParametersRunnerFactory(AwJUnit4ClassRunnerWithParameters.Factory.class)
@MinAndroidSdkLevel(Build.VERSION_CODES.P)
@RequiresApi(Build.VERSION_CODES.P)
@OnlyRunIn(EITHER_PROCESS) // These tests don't use the renderer process
public class AwPacProcessorTest extends AwParameterizedTest {
    private AwPacProcessor mProcessor;

    private final String mPacScript =
            "function FindProxyForURL(url, host) {\n"
                    + "var x = myIpAddress();"
                    + "\treturn \"PROXY \" + x + \":80\";\n"
                    + "}";
    private final String mTestUrl = "http://testurl.test";

    @Rule public AwActivityTestRule mRule;

    public AwPacProcessorTest(AwSettingsMutation param) {
        this.mRule = new AwActivityTestRule(param.getMutation());
    }

    @Before
    public void setUp() {
        JNIUtils.setClassLoader(WebViewChromiumFactoryProvider.class.getClassLoader());
        LibraryLoader.getInstance().ensureInitialized();

        mProcessor = AwPacProcessor.getInstance();
    }

    @Test
    @SmallTest
    public void testUpdateNetworkAndLinkAddresses() throws Throwable {
        // PAC script returns result of myIpAddress call
        mProcessor.setProxyScript(mPacScript);

        // Save the proxy request result when network is not set
        String proxyResultNetworkIsNotSet = mProcessor.makeProxyRequest(mTestUrl);

        // Set network and IP addresses, check they are correctly propagated.
        mProcessor.setNetworkAndLinkAddresses(42, new String[] {"1.2.3.4"});
        String proxyResultNetworkIsSet = mProcessor.makeProxyRequest(mTestUrl);
        Assert.assertEquals("PROXY 1.2.3.4:80", proxyResultNetworkIsSet);

        // Unset network, the returned proxy string must be equal previously saved value
        mProcessor.setNetwork(null);
        Assert.assertEquals(proxyResultNetworkIsNotSet, mProcessor.makeProxyRequest(mTestUrl));
    }
}
