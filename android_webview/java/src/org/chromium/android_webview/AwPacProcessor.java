// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview;

import android.content.Context;
import android.net.ConnectivityManager;
import android.net.LinkAddress;
import android.net.LinkProperties;
import android.net.Network;
import android.net.NetworkRequest;
import android.os.Build;

import androidx.annotation.RequiresApi;

import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.base.ContextUtils;
import org.chromium.build.annotations.UsedByReflection;

import java.net.InetAddress;

/**
 * Class to evaluate PAC scripts. Its lifecycle is independent of
 * any Renderer, Profile, or WebView instance.
 */
@JNINamespace("android_webview")
@RequiresApi(Build.VERSION_CODES.P)
// TODO(amalova): remove UsedByReflection
@UsedByReflection("Android")
public class AwPacProcessor {
    private long mNativePacProcessor;
    private Network mNetwork;
    private ConnectivityManager.NetworkCallback mNetworkCallback;

    public static final long NETWORK_UNSPECIFIED = 0;

    private static class LazyHolder {
        static final AwPacProcessor sInstance = new AwPacProcessor();
    }

    public static AwPacProcessor getInstance() {
        return LazyHolder.sInstance;
    }

    public AwPacProcessor() {
        mNativePacProcessor = AwPacProcessorJni.get().createNativePacProcessor();
    }

    private static ConnectivityManager getConnectivityManager() {
        Context context = ContextUtils.getApplicationContext();
        return (ConnectivityManager) context.getSystemService(Context.CONNECTIVITY_SERVICE);
    }

    private void updateNetworkLinkAddress(Network network, LinkProperties linkProperties) {
        if (network == null || linkProperties == null) {
            setNetworkAndLinkAddresses(NETWORK_UNSPECIFIED, new String[0]);
        } else {
            String[] addresses =
                    linkProperties.getLinkAddresses().stream()
                            .map(LinkAddress::getAddress)
                            .map(InetAddress::getHostAddress)
                            .toArray(String[]::new);
            setNetworkAndLinkAddresses(network.getNetworkHandle(), addresses);
        }
    }

    public void setNetworkAndLinkAddresses(long networkHandle, String[] addresses) {
        AwPacProcessorJni.get()
                .setNetworkAndLinkAddresses(mNativePacProcessor, networkHandle, addresses);
    }

    private void registerNetworkCallback() {
        if (mNetworkCallback != null) return;

        mNetworkCallback =
                new ConnectivityManager.NetworkCallback() {
                    @Override
                    public void onLinkPropertiesChanged(
                            Network network, LinkProperties linkProperties) {
                        if (network.equals(mNetwork)) {
                            updateNetworkLinkAddress(network, linkProperties);
                        }
                    }
                };
        NetworkRequest.Builder builder = new NetworkRequest.Builder();

        getConnectivityManager().registerNetworkCallback(builder.build(), mNetworkCallback);
    }

    private void unregisterNetworkCallback() {
        if (mNetworkCallback == null) return;

        getConnectivityManager().unregisterNetworkCallback(mNetworkCallback);
        mNetworkCallback = null;
    }

    // The calling code must not call any methods after it called destroy().
    @UsedByReflection("Android")
    public void destroy() {
        unregisterNetworkCallback();
        AwPacProcessorJni.get().destroyNative(mNativePacProcessor, this);
    }

    @UsedByReflection("Android")
    public boolean setProxyScript(String script) {
        return AwPacProcessorJni.get().setProxyScript(mNativePacProcessor, this, script);
    }

    @UsedByReflection("Android")
    public String makeProxyRequest(String url) {
        return AwPacProcessorJni.get().makeProxyRequest(mNativePacProcessor, this, url);
    }

    @UsedByReflection("Android")
    public void setNetwork(Network network) {
        mNetwork = network;
        if (mNetwork != null) {
            registerNetworkCallback();
        } else {
            unregisterNetworkCallback();
        }
        updateNetworkLinkAddress(network, getConnectivityManager().getLinkProperties(network));
    }

    @UsedByReflection("Android")
    public Network getNetwork() {
        return mNetwork;
    }

    public static void initializeEnvironment() {
        AwPacProcessorJni.get().initializeEnvironment();
    }

    @NativeMethods
    interface Natives {
        void initializeEnvironment();

        long createNativePacProcessor();

        boolean setProxyScript(long nativeAwPacProcessor, AwPacProcessor caller, String script);

        String makeProxyRequest(long nativeAwPacProcessor, AwPacProcessor caller, String url);

        void destroyNative(long nativeAwPacProcessor, AwPacProcessor caller);

        void setNetworkAndLinkAddresses(
                long nativeAwPacProcessor, long networkHandle, String[] adresses);
    }
}
