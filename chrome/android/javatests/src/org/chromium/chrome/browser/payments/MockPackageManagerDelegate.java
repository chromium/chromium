// Copyright 2017 The Chromium Authors. All rights reserved.
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

import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

/** Simulates a package manager in memory. */
class MockPackageManagerDelegate extends PackageManagerDelegate {
    private static final int STRING_ARRAY_RESOURCE_ID = 1;

    private final List<ResolveInfo> mActivities = new ArrayList<>();
    private final Map<String, PackageInfo> mPackages = new HashMap<>();
    private final Map<ResolveInfo, CharSequence> mLabels = new HashMap<>();
    private final List<ResolveInfo> mServices = new ArrayList<>();
    private final Map<ApplicationInfo, String[]> mResources = new HashMap<>();

    /**
     * Simulates an installed payment app.
     *
     * @param label                    The user visible name of the app.
     * @param packageName              The identifying package name.
     * @param defaultPaymentMethodName The name of the default payment method name for this app.
     *                                 If null, then this app will not have metadata. If empty,
     *                                 then the default payment method name will not be set.
     * @param signature                The signature of the app. The SHA256 hash of this
     *                                 signature is called "fingerprint" and should be present in
     *                                 the app's web app manifest. If null, then this app will
     *                                 not have package info. If empty, then this app will not
     *                                 have any signatures.
     */
    public void installPaymentApp(CharSequence label, String packageName,
            String defaultPaymentMethodName, String signature) {
        ResolveInfo paymentApp = new ResolveInfo();
        paymentApp.activityInfo = new ActivityInfo();
        paymentApp.activityInfo.packageName = packageName;
        paymentApp.activityInfo.name = packageName + ".WebPaymentActivity";
        paymentApp.activityInfo.applicationInfo = new ApplicationInfo();
        if (defaultPaymentMethodName != null) {
            Bundle metaData = new Bundle();
            if (!defaultPaymentMethodName.isEmpty()) {
                metaData.putString(
                        AndroidPaymentAppFinder.META_DATA_NAME_OF_DEFAULT_PAYMENT_METHOD_NAME,
                        defaultPaymentMethodName);
            }
            paymentApp.activityInfo.metaData = metaData;
        }
        mActivities.add(paymentApp);

        if (signature != null) {
            PackageInfo packageInfo = new PackageInfo();
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
     * @param metadata    The metadata to simulate.
     */
    public void setStringArrayMetaData(String packageName, String[] metadata) {
        for (int i = 0; i < mActivities.size(); i++) {
            ResolveInfo paymentApp = mActivities.get(i);
            if (paymentApp.activityInfo.packageName.equals(packageName)) {
                paymentApp.activityInfo.metaData.putInt(
                        AndroidPaymentAppFinder.META_DATA_NAME_OF_PAYMENT_METHOD_NAMES,
                        STRING_ARRAY_RESOURCE_ID);
                mResources.put(paymentApp.activityInfo.applicationInfo, metadata);
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
        assert STRING_ARRAY_RESOURCE_ID == resourceId;
        return mResources.get(applicationInfo);
    }
}