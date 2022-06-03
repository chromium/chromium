// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.sharing.shared_clipboard;

import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.view.View;
import android.view.animation.AnimationUtils;
import android.widget.AdapterView;
import android.widget.AdapterView.OnItemClickListener;
import android.widget.ListView;

import org.chromium.base.ContextUtils;
import org.chromium.base.IntentUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.init.AsyncInitializationActivity;
import org.chromium.chrome.browser.settings.SettingsLauncherImpl;
import org.chromium.chrome.browser.sharing.SharingAdapter;
import org.chromium.chrome.browser.sharing.SharingServiceProxy;
import org.chromium.chrome.browser.sharing.SharingServiceProxy.DeviceInfo;
import org.chromium.chrome.browser.sync.AndroidSyncSettings;
import org.chromium.components.browser_ui.settings.SettingsLauncher;
import org.chromium.components.sync.protocol.SharingSpecificFields;
import org.chromium.ui.widget.ButtonCompat;

/**
 * Activity to display device targets to share text.
 */
public class SharedClipboardShareActivity
        extends AsyncInitializationActivity implements OnItemClickListener {
    private SharingAdapter mAdapter;

    /**
     * Checks whether sending shared clipboard message is enabled for the user and enables/disables
     * the SharedClipboardShareActivity appropriately. This call requires native to be loaded.
     */
    public static void updateComponentEnabledState() {
        boolean enabled = ChromeFeatureList.isEnabled(ChromeFeatureList.SHARED_CLIPBOARD_UI);
        PostTask.postTask(TaskTraits.USER_VISIBLE, () -> setComponentEnabled(enabled));
    }

    /**
     * Sets whether or not the SharedClipboardShareActivity should be enabled. This may trigger a
     * StrictMode violation so shouldn't be called on the UI thread.
     */
    private static void setComponentEnabled(boolean enabled) {
        ThreadUtils.assertOnBackgroundThread();
        Context context = ContextUtils.getApplicationContext();
        PackageManager packageManager = context.getPackageManager();
        ComponentName componentName =
                new ComponentName(context, SharedClipboardShareActivity.class);

        int newState = enabled ? PackageManager.COMPONENT_ENABLED_STATE_ENABLED
                               : PackageManager.COMPONENT_ENABLED_STATE_DISABLED;

        // This indicates that we don't want to kill Chrome when changing component enabled state.
        int flags = PackageManager.DONT_KILL_APP;

        if (packageManager.getComponentEnabledSetting(componentName) != newState) {
            packageManager.setComponentEnabledSetting(componentName, newState, flags);
        }
    }

    @Override
    protected void triggerLayoutInflation() {
        setContentView(R.layout.sharing_device_picker);

        View mask = findViewById(R.id.mask);
        mask.setOnClickListener(v -> finish());

        ButtonCompat chromeSettingsButton = findViewById(R.id.chrome_settings);
        if (!AndroidSyncSettings.get().isChromeSyncEnabled()) {
            chromeSettingsButton.setVisibility(View.VISIBLE);
            chromeSettingsButton.setOnClickListener(view -> {
                SettingsLauncher settingsLauncher = new SettingsLauncherImpl();
                settingsLauncher.launchSettingsActivity(ContextUtils.getApplicationContext());
            });
        }

        onInitialLayoutInflationComplete();
    }

    @Override
    public void startNativeInitialization() {
        SharingServiceProxy.getInstance().addDeviceCandidatesInitializedObserver(
                this::finishNativeInitialization);
    }

    @Override
    public void finishNativeInitialization() {
        super.finishNativeInitialization();

        mAdapter = new SharingAdapter(SharingSpecificFields.EnabledFeatures.SHARED_CLIPBOARD_V2);
        if (!mAdapter.isEmpty()) {
            findViewById(R.id.device_picker_toolbar).setVisibility(View.VISIBLE);
            SharedClipboardMetrics.recordShowDeviceList();
        } else {
            SharedClipboardMetrics.recordShowEducationalDialog();
        }

        SharedClipboardMetrics.recordDeviceCount(mAdapter.getCount());

        ListView listView = findViewById(R.id.device_picker_list);
        listView.setAdapter(mAdapter);
        listView.setOnItemClickListener(this);
        listView.setEmptyView(findViewById(R.id.empty_state));

        View content = findViewById(R.id.device_picker_content);
        content.startAnimation(AnimationUtils.loadAnimation(this, R.anim.slide_in_up));
    }

    @Override
    public boolean shouldStartGpuProcess() {
        return false;
    }

    @Override
    public void onItemClick(AdapterView<?> parent, View view, int position, long id) {
        DeviceInfo device = mAdapter.getItem(position);
        String text = IntentUtils.safeGetStringExtra(getIntent(), Intent.EXTRA_TEXT);

        // Log metrics for device click and text size.
        SharedClipboardMetrics.recordDeviceClick(position);
        SharedClipboardMetrics.recordTextSize(text != null ? text.length() : 0);

        SharedClipboardMessageHandler.showSendingNotification(device.guid, device.clientName, text);
        finish();
    }
}
