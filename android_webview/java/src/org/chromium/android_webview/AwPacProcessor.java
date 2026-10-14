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

import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.base.ContextUtils;
import org.chromium.build.annotations.Nullable;
import org.chromium.build.annotations.UsedByReflection;

import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.locks.Lock;
import java.util.concurrent.locks.ReentrantReadWriteLock;

import javax.annotation.concurrent.GuardedBy;

/**
 * Class to evaluate PAC scripts. Its lifecycle is independent of any Renderer, Profile, or WebView
 * instance.
 */
@JNINamespace("android_webview")
// TODO(amalova): remove UsedByReflection
@UsedByReflection("Android")
public class AwPacProcessor {
    // Guards all the fields below. PAC evaluations, which can be slow, hold the read lock so that
    // they can run concurrently. destroy() and network changes hold the write lock. It's fair so
    // that new evaluations can't keep a writer waiting.
    private final ReentrantReadWriteLock mLock = new ReentrantReadWriteLock(/* fair= */ true);

    // Lock with these instead of mLock directly, so that Error Prone can check @GuardedBy.
    private final Lock mReadLock = mLock.readLock();
    private final Lock mWriteLock = mLock.writeLock();

    // 0 if it's already been destroyed.
    @GuardedBy("mLock")
    private long mNativePacProcessor;

    @GuardedBy("mLock")
    private Network mNetwork;

    @GuardedBy("mWriteLock")
    private ConnectivityManager.NetworkCallback mNetworkCallback;

    public static final long NETWORK_UNSPECIFIED = 0;

    private static class LazyHolder {
        static final AwPacProcessor sInstance = new AwPacProcessor();
    }

    public static AwPacProcessor getInstance() {
        return LazyHolder.sInstance;
    }

    public AwPacProcessor() {
        // Makes mNativePacProcessor visible to other threads, which read it under mLock.
        mWriteLock.lock();
        try {
            mNativePacProcessor = AwPacProcessorJni.get().createNativePacProcessor();
        } finally {
            mWriteLock.unlock();
        }
    }

    private static ConnectivityManager getConnectivityManager() {
        Context context = ContextUtils.getApplicationContext();
        return (ConnectivityManager) context.getSystemService(Context.CONNECTIVITY_SERVICE);
    }

    @GuardedBy("mWriteLock")
    private void updateNetworkLinkAddressLocked(Network network, LinkProperties linkProperties) {
        long networkHandle = NETWORK_UNSPECIFIED;
        ArrayList<String> addresses = new ArrayList<>();
        if (network != null
                && linkProperties != null
                && !linkProperties.getLinkAddresses().isEmpty()) {
            networkHandle = network.getNetworkHandle();
            for (LinkAddress addr : linkProperties.getLinkAddresses()) {
                addresses.add(addr.getAddress().getHostAddress());
            }
        }
        setNetworkAndLinkAddressesLocked(networkHandle, addresses);
    }

    // TODO(ziadyoussef): Rename to setNetworkAndLinkAddressesForTesting(). Only tests call it,
    // and it isn't @UsedByReflection, so the framework can't call it either.
    public void setNetworkAndLinkAddresses(long networkHandle, List<String> addresses) {
        mWriteLock.lock();
        try {
            setNetworkAndLinkAddressesLocked(networkHandle, addresses);
        } finally {
            mWriteLock.unlock();
        }
    }

    @GuardedBy("mWriteLock")
    private void setNetworkAndLinkAddressesLocked(long networkHandle, List<String> addresses) {
        if (mNativePacProcessor == 0) return;
        AwPacProcessorJni.get()
                .setNetworkAndLinkAddresses(mNativePacProcessor, networkHandle, addresses);
    }

    @GuardedBy("mWriteLock")
    private void registerNetworkCallbackLocked() {
        if (mNetworkCallback != null) return;

        mNetworkCallback =
                new ConnectivityManager.NetworkCallback() {
                    @Override
                    public void onLinkPropertiesChanged(
                            Network network, LinkProperties linkProperties) {
                        mWriteLock.lock();
                        try {
                            if (!network.equals(mNetwork)) return;
                            updateNetworkLinkAddressLocked(network, linkProperties);
                        } finally {
                            mWriteLock.unlock();
                        }
                    }
                };
        NetworkRequest.Builder builder = new NetworkRequest.Builder();

        getConnectivityManager().registerNetworkCallback(builder.build(), mNetworkCallback);
    }

    @GuardedBy("mWriteLock")
    private void unregisterNetworkCallbackLocked() {
        if (mNetworkCallback == null) return;

        getConnectivityManager().unregisterNetworkCallback(mNetworkCallback);
        mNetworkCallback = null;
    }

    // The calling code must not call any methods after it called destroy().
    @UsedByReflection("Android")
    public void destroy() {
        // Fail in-flight and new PAC evaluations to minimize the wait for the write lock below.
        mReadLock.lock();
        try {
            if (mNativePacProcessor == 0) return;
            AwPacProcessorJni.get().cancelJobs(mNativePacProcessor);
        } finally {
            mReadLock.unlock();
        }

        mWriteLock.lock();
        try {
            if (mNativePacProcessor == 0) return;
            long nativePacProcessor = mNativePacProcessor;
            mNativePacProcessor = 0;
            unregisterNetworkCallbackLocked();
            // This deletes the native object on the PAC thread, after the tasks already posted
            // there, and doesn't wait for it.
            AwPacProcessorJni.get().destroyNative(nativePacProcessor);
        } finally {
            mWriteLock.unlock();
        }
    }

    @UsedByReflection("Android")
    public boolean setProxyScript(String script) {
        mReadLock.lock();
        try {
            if (mNativePacProcessor == 0) return false;
            return AwPacProcessorJni.get().setProxyScript(mNativePacProcessor, script);
        } finally {
            mReadLock.unlock();
        }
    }

    @UsedByReflection("Android")
    public String makeProxyRequest(String url) {
        mReadLock.lock();
        try {
            if (mNativePacProcessor == 0) return null;
            return AwPacProcessorJni.get().makeProxyRequest(mNativePacProcessor, url);
        } finally {
            mReadLock.unlock();
        }
    }

    @UsedByReflection("Android")
    public void setNetwork(Network network) {
        mWriteLock.lock();
        try {
            // Don't register a network callback which would never be unregistered.
            if (mNativePacProcessor == 0) return;
            mNetwork = network;
            if (mNetwork != null) {
                registerNetworkCallbackLocked();
            } else {
                unregisterNetworkCallbackLocked();
            }
            updateNetworkLinkAddressLocked(
                    network, getConnectivityManager().getLinkProperties(network));
        } finally {
            mWriteLock.unlock();
        }
    }

    @UsedByReflection("Android")
    public Network getNetwork() {
        mReadLock.lock();
        try {
            return mNetwork;
        } finally {
            mReadLock.unlock();
        }
    }

    public static void initializeEnvironment() {
        AwPacProcessorJni.get().initializeEnvironment();
    }

    @NativeMethods
    interface Natives {
        void initializeEnvironment();

        long createNativePacProcessor();

        boolean setProxyScript(long nativeAwPacProcessor, @JniType("std::string") String script);

        @JniType("std::optional<std::string>")
        @Nullable String makeProxyRequest(
                long nativeAwPacProcessor, @JniType("std::string") String url);

        void destroyNative(long nativeAwPacProcessor);

        void cancelJobs(long nativeAwPacProcessor);

        void setNetworkAndLinkAddresses(
                long nativeAwPacProcessor,
                long networkHandle,
                @JniType("std::vector<std::string>") List<String> addresses);
    }
}
