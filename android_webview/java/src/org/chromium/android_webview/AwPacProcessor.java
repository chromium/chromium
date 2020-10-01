// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview;

import android.annotation.TargetApi;
import android.content.Context;
import android.net.ConnectivityManager;
import android.net.LinkAddress;
import android.net.LinkProperties;
import android.net.Network;
import android.net.NetworkRequest;

import org.chromium.base.ContextUtils;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;

/**
 * Class to evaluate PAC scripts.
 */
@JNINamespace("android_webview")
@TargetApi(28)
public class AwPacProcessor {
    private long mNativePacProcessor;
    private Network mNetwork;

    public static final long NETWORK_UNSPECIFIED = 0;

    private static class LazyHolder {
        static final AwPacProcessor sInstance = new AwPacProcessor(null);
    }

    public static AwPacProcessor getInstance() {
        return LazyHolder.sInstance;
    }

    public AwPacProcessor(Network network) {
        if (network == null) {
            mNativePacProcessor =
                    AwPacProcessorJni.get().createNativePacProcessor(NETWORK_UNSPECIFIED);
            return;
        }

        mNetwork = network;
        mNativePacProcessor =
                AwPacProcessorJni.get().createNativePacProcessor(mNetwork.getNetworkHandle());

        Context context = ContextUtils.getApplicationContext();
        ConnectivityManager connectivityManager =
                (ConnectivityManager) context.getSystemService(Context.CONNECTIVITY_SERVICE);
        NetworkRequest.Builder builder = new NetworkRequest.Builder();

        connectivityManager.registerNetworkCallback(
                builder.build(), new ConnectivityManager.NetworkCallback() {
                    @Override
                    public void onLinkPropertiesChanged(
                            Network network, LinkProperties linkProperties) {
                        if (network.equals(mNetwork)) {
                            updateNetworkLinkAddress(linkProperties);
                        }
                    }
                });

        updateNetworkLinkAddress(connectivityManager.getLinkProperties(mNetwork));
    }

    private void updateNetworkLinkAddress(LinkProperties linkProperties) {
        String[] addresses = linkProperties.getLinkAddresses()
                                     .stream()
                                     .map(LinkAddress::toString)
                                     .toArray(String[] ::new);
        AwPacProcessorJni.get().setNetworkLinkAddresses(mNativePacProcessor, addresses);
    }

    // The calling code must not call any methods after it called destroy().
    public void destroy() {
        AwPacProcessorJni.get().destroyNative(mNativePacProcessor, this);
    }

    public boolean setProxyScript(String script) {
        return AwPacProcessorJni.get().setProxyScript(mNativePacProcessor, this, script);
    }

    public String makeProxyRequest(String url) {
        return AwPacProcessorJni.get().makeProxyRequest(mNativePacProcessor, this, url);
    }

    public Network getNetwork() {
        return mNetwork;
    }

    public static void initializeEnvironment() {
        AwPacProcessorJni.get().initializeEnvironment();
    }

    @NativeMethods
    interface Natives {
        void initializeEnvironment();
        long createNativePacProcessor(long netHandle);
        boolean setProxyScript(long nativeAwPacProcessor, AwPacProcessor caller, String script);
        String makeProxyRequest(long nativeAwPacProcessor, AwPacProcessor caller, String url);
        void destroyNative(long nativeAwPacProcessor, AwPacProcessor caller);
        void setNetworkLinkAddresses(long nativeAwPacProcessor, String[] adresses);
    }
}
