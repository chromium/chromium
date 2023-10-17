// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments;

import android.content.Intent;
import android.content.pm.ActivityInfo;
import android.content.pm.ApplicationInfo;
import android.content.pm.PackageInfo;
import android.content.pm.ResolveInfo;
import android.content.pm.ServiceInfo;
import android.content.pm.Signature;
import android.graphics.drawable.Drawable;
import android.os.Bundle;

import androidx.annotation.Nullable;

import org.chromium.components.payments.AndroidPaymentAppFinder;
import org.chromium.components.payments.PackageManagerDelegate;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

/** Simulates a package manager in memory. */
class MockPackageManagerDelegate extends PackageManagerDelegate {
    private static final int PAYMENT_METHOD_NAMES_STRING_ARRAY_RESOURCE_ID = 1;
    private static final int SUPPORTED_DELEGATIONS_STRING_ARRAY_RESOURCE_ID = 2;
    private static final int RESOURCES_SIZE = 2;

    private final List<ResolveInfo> mActivities = new ArrayList<>();
    private final Map<String, PackageInfo> mPackages = new HashMap<>();
    private final Map<ResolveInfo, CharSequence> mLabels = new HashMap<>();
    private final List<ResolveInfo> mServices = new ArrayList<>();
    private final Map<ApplicationInfo, List<String[]>> mResources = new HashMap<>();

    private String mInvokedAppPackageName;

    // A map of a package name to its installer's package name.
    private Map<String, String> mMockInstallerPackageMap = new HashMap<>();

    /**
     * Simulates an installed payment app with no supported delegations.
     *
     * @param label The user visible name of the app.
     * @param packageName The identifying package name.
     * @param defaultPaymentMethodName The name of the default payment method name for this app. If
     *     null, then this app will not have metadata. If empty, then the default payment method
     *     name will not be set.
     * @param signature The signature of the app. The SHA256 hash of this signature is called
     *     "fingerprint" and should be present in the app's web app manifest. If null, then this app
     *     will not have package info. If empty, then this app will not have any signatures.
     */
    public void installPaymentApp(
            CharSequence label,
            String packageName,
            String defaultPaymentMethodName,
            String signature) {
        installPaymentApp(
                label,
                packageName,
                defaultPaymentMethodName,
                /* supportedDelegations= */ null,
                signature);
    }

    /**
     * Simulates an installed payment app.
     *
     * @param label The user visible name of the app.
     * @param packageName The identifying package name.
     * @param defaultPaymentMethodName The name of the default payment method name for this app. If
     *     empty, then the default payment method name will not be set.
     * @param supportedDelegations The delegations that the app can support. If both
     *     supportedDelegations and defaultPaymentMethodName null, then this app will not have
     *     metadata.
     * @param signature The signature of the app. The SHA256 hash of this signature is called
     *     "fingerprint" and should be present in the app's web app manifest. If null, then this app
     *     will not have package info. If empty, then this app will not have any signatures.
     */
    public void installPaymentApp(
            CharSequence label,
            String packageName,
            String defaultPaymentMethodName,
            String[] supportedDelegations,
            String signature) {
        ResolveInfo paymentApp = new ResolveInfo();
        paymentApp.activityInfo = new ActivityInfo();
        paymentApp.activityInfo.packageName = packageName;
        paymentApp.activityInfo.name = packageName + ".WebPaymentActivity";
        paymentApp.activityInfo.applicationInfo = new ApplicationInfo();
        if (defaultPaymentMethodName != null || supportedDelegations != null) {
            Bundle metaData = new Bundle();
            if (!defaultPaymentMethodName.isEmpty()) {
                metaData.putString(
                        AndroidPaymentAppFinder.META_DATA_NAME_OF_DEFAULT_PAYMENT_METHOD_NAME,
                        defaultPaymentMethodName);
            }
            if (supportedDelegations != null && supportedDelegations.length > 0) {
                metaData.putInt(
                        AndroidPaymentAppFinder.META_DATA_NAME_OF_SUPPORTED_DELEGATIONS,
                        SUPPORTED_DELEGATIONS_STRING_ARRAY_RESOURCE_ID);
                List<String[]> resources = Arrays.asList(new String[RESOURCES_SIZE][]);
                resources.set(
                        SUPPORTED_DELEGATIONS_STRING_ARRAY_RESOURCE_ID - 1, supportedDelegations);
                mResources.put(paymentApp.activityInfo.applicationInfo, resources);
            }
            paymentApp.activityInfo.metaData = metaData;
        }
        mActivities.add(paymentApp);

        if (signature != null) {
            PackageInfo packageInfo = new PackageInfo();
            packageInfo.packageName = packageName;
            packageInfo.versionCode = 10;
            if (signature.isEmpty()) {
                packageInfo.signatures = new Signature[0];
            } else {
                packageInfo.signatures = new Signature[1];
                packageInfo.signatures[0] = new Signature(signature);
            }
            mPackages.put(packageName, packageInfo);
        }

        mLabels.put(paymentApp, label);
    }

    /**
     * Simulates an IS_READY_TO_PAY service in a payment app.
     *
     * @param packageName The identifying package name.
     */
    public void addIsReadyToPayService(String packageName) {
        ResolveInfo service = new ResolveInfo();
        service.serviceInfo = new ServiceInfo();
        service.serviceInfo.packageName = packageName;
        service.serviceInfo.name = packageName + ".IsReadyToPayService";
        mServices.add(service);
    }

    /**
     * Simulates META_DATA_NAME_OF_PAYMENT_METHOD_NAMES metadata in a payment app.
     *
     * @param packageName The name of the simulated package that contains the metadata.
     * @param metadata The metadata to simulate.
     */
    public void setStringArrayMetaData(String packageName, String[] metadata) {
        for (int i = 0; i < mActivities.size(); i++) {
            ResolveInfo paymentApp = mActivities.get(i);
            if (paymentApp.activityInfo.packageName.equals(packageName)) {
                paymentApp.activityInfo.metaData.putInt(
                        AndroidPaymentAppFinder.META_DATA_NAME_OF_PAYMENT_METHOD_NAMES,
                        PAYMENT_METHOD_NAMES_STRING_ARRAY_RESOURCE_ID);
                List<String[]> resources;
                if (mResources.containsKey(paymentApp.activityInfo.applicationInfo)) {
                    resources = mResources.get(paymentApp.activityInfo.applicationInfo);
                    mResources.remove(paymentApp.activityInfo.applicationInfo);
                } else {
                    resources = Arrays.asList(new String[RESOURCES_SIZE][]);
                }
                resources.set(PAYMENT_METHOD_NAMES_STRING_ARRAY_RESOURCE_ID - 1, metadata);
                mResources.put(paymentApp.activityInfo.applicationInfo, resources);
                return;
            }
        }
        assert false : packageName + " package not found";
    }

    /** Resets the package manager to the state of no installed apps. */
    public void reset() {
        mActivities.clear();
        mPackages.clear();
        mLabels.clear();
    }

    @Override
    public List<ResolveInfo> getActivitiesThatCanRespondToIntentWithMetaData(Intent intent) {
        return mActivities;
    }

    @Override
    public List<ResolveInfo> getActivitiesThatCanRespondToIntent(Intent intent) {
        return getActivitiesThatCanRespondToIntentWithMetaData(intent);
    }

    @Override
    public List<ResolveInfo> getServicesThatCanRespondToIntent(Intent intent) {
        return mServices;
    }

    @Override
    public PackageInfo getPackageInfoWithSignatures(String packageName) {
        return mPackages.get(packageName);
    }

    @Override
    public PackageInfo getPackageInfoWithSignatures(int uid) {
        return mPackages.get(mInvokedAppPackageName);
    }

    @Override
    public CharSequence getAppLabel(ResolveInfo resolveInfo) {
        return mLabels.get(resolveInfo);
    }

    @Override
    public Drawable getAppIcon(ResolveInfo resolveInfo) {
        return null;
    }

    @Override
    @Nullable
    public String[] getStringArrayResourceForApplication(
            ApplicationInfo applicationInfo, int resourceId) {
        assert resourceId > 0 && resourceId <= RESOURCES_SIZE;
        return mResources.get(applicationInfo).get(resourceId - 1);
    }

    /**
     * Sets the package name of the invoked payment app.
     *
     * @param packageName The package name of the invoked payment app.
     */
    public void setInvokedAppPackageName(String packageName) {
        assert mPackages.containsKey(packageName);
        mInvokedAppPackageName = packageName;
    }

    @Override
    public @Nullable String getInstallerPackage(String packageName) {
        return mMockInstallerPackageMap.get(packageName);
    }

    /**
     * Mock the installer of a specified package.
     *
     * @param packageName The package name that is intended to mock a installer for, not allowed to
     *     be null.
     * @param installerPackageName The package name intended to be set as the installer of the
     *     specified package.
     */
    public void mockInstallerForPackage(String packageName, @Nullable String installerPackageName) {
        assert packageName != null;
        mMockInstallerPackageMap.put(packageName, installerPackageName);
    }
}
