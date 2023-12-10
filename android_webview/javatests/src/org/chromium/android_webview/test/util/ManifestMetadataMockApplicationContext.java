// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test.util;

import android.content.ComponentName;
import android.content.Context;
import android.content.pm.ApplicationInfo;
import android.content.pm.PackageManager;
import android.content.pm.ServiceInfo;
import android.content.res.Resources;
import android.os.Bundle;

import org.mockito.Mockito;

import org.chromium.base.test.util.ApplicationContextWrapper;
import org.chromium.base.test.util.PackageManagerWrapper;

import java.util.HashMap;
import java.util.Map;

/**
 * Mock ApplicationContext that allows you to inject meta-data for a service
 * and resources.
 */
public class ManifestMetadataMockApplicationContext extends ApplicationContextWrapper {
    Map<String, Bundle> mApplicationMetadata = new HashMap<>();
    Map<ComponentName, Bundle> mServiceMetadata = new HashMap<>();
    Map<Integer, String[]> mStringArrayResources = new HashMap<>();

    /**
     * Construct a new Mock context, wrapping the passed (real) applicationContexxt
     * @param applicationContext The real context to wrap
     */
    public ManifestMetadataMockApplicationContext(final Context applicationContext) {
        super(applicationContext);
    }

    /**
     * Put a new metadata bundle into the context for the application package.
     *
     * The value will be returned as {@code getPackageManager().getApplicationInfo().metaData}
     * @param packageName Package name of the application
     * @param metadata Metadata to return
     */
    public void putServiceMetadata(String packageName, Bundle metadata) {
        mApplicationMetadata.put(packageName, metadata);
    }

    /**
     * Put a new metadata bundle into the context for the given service component.
     *
     * The value will be returned as {@code getPackageManager().getServiceInfo().metaData}
     * @param name ComponentName of the service
     * @param metadata Metadata to return
     */
    public void putServiceMetadata(ComponentName name, Bundle metadata) {
        mServiceMetadata.put(name, metadata);
    }

    /**
     * Add a new StringArray resource with the given ID.
     *
     * Values will be returned through {@code getResources().getStringArray()}
     *
     * @param id Resource ID
     * @param value String array value to return.
     */
    public void putStringArrayResource(int id, String[] value) {
        mStringArrayResources.put(id, value);
    }

    @Override
    public Resources getResources() {
        // The resources class is not built to be easily wrapped.
        // Create a wrapping spy instead.
        Resources resourcesSpy = Mockito.spy(super.getResources());
        // Use the "doAnswer-when" ordering since the real method call throws an exception
        // which breaks the more fluent "when-then" call pattern.
        Mockito.doAnswer(
                        invocation -> {
                            Integer key = invocation.getArgument(0);
                            if (mStringArrayResources.containsKey(key)) {
                                return mStringArrayResources.get(key);
                            }
                            return invocation.callRealMethod();
                        })
                .when(resourcesSpy)
                .getStringArray(Mockito.anyInt());
        return resourcesSpy;
    }

    @Override
    public PackageManager getPackageManager() {
        return new PackageManagerWrapper(super.getPackageManager()) {
            @Override
            public ApplicationInfo getApplicationInfo(String packageName, int flags)
                    throws NameNotFoundException {
                var applicationInfo = super.getApplicationInfo(packageName, flags);
                if (mApplicationMetadata.containsKey(packageName)) {
                    var injectedBundle = mApplicationMetadata.get(packageName);
                    if (applicationInfo.metaData != null) {
                        applicationInfo.metaData.putAll(injectedBundle);
                    } else {
                        applicationInfo.metaData = injectedBundle;
                    }
                }
                return applicationInfo;
            }

            @Override
            public ServiceInfo getServiceInfo(ComponentName component, int flags)
                    throws NameNotFoundException {
                if (mServiceMetadata.containsKey(component)) {
                    ServiceInfo serviceInfo = new ServiceInfo();
                    serviceInfo.metaData = mServiceMetadata.get(component);
                    return serviceInfo;
                }
                return super.getServiceInfo(component, flags);
            }
        };
    }
}
