// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.settings;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.Color;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.ColorDrawable;
import android.graphics.drawable.Drawable;
import android.os.Bundle;
import android.util.Pair;
import android.view.View;

import androidx.preference.PreferenceFragmentCompat;
import androidx.preference.PreferenceScreen;

import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.payments.ServiceWorkerPaymentAppBridge;
import org.chromium.components.browser_ui.settings.EmbeddableSettingsPage;
import org.chromium.components.browser_ui.settings.TextMessagePreference;
import org.chromium.components.payments.AndroidPaymentAppFactory;

import java.util.Map;

/** Preference fragment to allow users to control use of the Android payment apps on device. */
public class AndroidPaymentAppsFragment extends PreferenceFragmentCompat
        implements EmbeddableSettingsPage {
    private final ObservableSupplierImpl<String> mPageTitle = new ObservableSupplierImpl<>();

    @Override
    public void onCreatePreferences(Bundle savedInstanceState, String rootKey) {
        mPageTitle.set(getString(R.string.payment_apps_title));

        // Create blank preference screen.
        PreferenceScreen screen = getPreferenceManager().createPreferenceScreen(getStyledContext());
        setPreferenceScreen(screen);
    }

    @Override
    public ObservableSupplier<String> getPageTitle() {
        return mPageTitle;
    }

    @Override
    public void onViewCreated(View view, Bundle savedInstanceState) {
        super.onViewCreated(view, savedInstanceState);

        // Disable animations of preference changes (crbug.com/986241).
        getListView().setItemAnimator(null);
    }

    @Override
    public void onResume() {
        super.onResume();
        rebuildPaymentAppsList();
    }

    private void rebuildPaymentAppsList() {
        getPreferenceScreen().removeAll();
        getPreferenceScreen().setOrderingAsAdded(true);

        ServiceWorkerPaymentAppBridge.getServiceWorkerPaymentAppsInfo(
                new ServiceWorkerPaymentAppBridge.GetServiceWorkerPaymentAppsInfoCallback() {
                    @Override
                    public void onGetServiceWorkerPaymentAppsInfo(
                            Map<String, Pair<String, Bitmap>> appsInfo) {
                        addPaymentAppsPreference(
                                AndroidPaymentAppFactory.getAndroidPaymentAppsInfo(), appsInfo);
                    }
                });
    }

    private void addPaymentAppsPreference(
            Map<String, Pair<String, Drawable>> androidAppsInfo,
            Map<String, Pair<String, Bitmap>> serviceWorkerAppsInfo) {
        if (androidAppsInfo.isEmpty() && serviceWorkerAppsInfo.isEmpty()) return;

        for (Map.Entry<String, Pair<String, Drawable>> app : androidAppsInfo.entrySet()) {
            AndroidPaymentAppPreference pref = new AndroidPaymentAppPreference(getStyledContext());
            pref.setTitle(app.getValue().first);
            pref.setIcon(app.getValue().second);
            getPreferenceScreen().addPreference(pref);
        }
        for (Map.Entry<String, Pair<String, Bitmap>> app : serviceWorkerAppsInfo.entrySet()) {
            AndroidPaymentAppPreference pref = new AndroidPaymentAppPreference(getStyledContext());
            pref.setTitle(app.getValue().first);
            pref.setSummary(app.getKey());
            pref.setIcon(
                    app.getValue().second == null
                            ? new ColorDrawable(Color.TRANSPARENT)
                            : new BitmapDrawable(getResources(), app.getValue().second));
            getPreferenceScreen().addPreference(pref);
        }

        TextMessagePreference textPreference = new TextMessagePreference(getStyledContext(), null);
        textPreference.setTitle(R.string.payment_apps_usage_message);
        textPreference.setDividerAllowedBelow(false);
        getPreferenceScreen().addPreference(textPreference);
    }

    private Context getStyledContext() {
        return getPreferenceManager().getContext();
    }
}
