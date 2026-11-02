// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share.send_tab_to_self;

import android.content.Intent;
import android.view.WindowManager;

import org.chromium.base.supplier.OneshotSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.init.ActivityProfileProvider;
import org.chromium.chrome.browser.init.AsyncInitializationActivity;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileProvider;

/**
 * Translucent activity that handles the "Send Tab to Self" direct share target intent. It
 * initializes native library asynchronously, sends the tab, and finishes.
 */
@NullMarked
public class SendTabToSelfShareTargetActivity extends AsyncInitializationActivity {
    @Override
    protected void performPreInflationStartup() {
        super.performPreInflationStartup();
        // Ensure this invisible activity never intercepts touch or key events in the underlying
        // app while waiting for the send operation to complete. Setting alpha = 0f is also required
        // on Android 12+ (API 31+) so InputDispatcher does not block cross-UID touches passing
        // through this FLAG_NOT_TOUCHABLE window.
        WindowManager.LayoutParams params = getWindow().getAttributes();
        params.flags |=
                WindowManager.LayoutParams.FLAG_NOT_TOUCHABLE
                        | WindowManager.LayoutParams.FLAG_NOT_FOCUSABLE;
        params.alpha = 0f;
        getWindow().setAttributes(params);
    }

    @Override
    protected void triggerLayoutInflation() {
        // This activity has no layout. Signal inflation completion immediately.
        onInitialLayoutInflationComplete();
    }

    @Override
    protected OneshotSupplier<ProfileProvider> createProfileProvider() {
        return new ActivityProfileProvider(getLifecycleDispatcher());
    }

    @Override
    public boolean shouldStartGpuProcess() {
        return false;
    }

    @Override
    public void finishNativeInitialization() {
        super.finishNativeInitialization();

        Intent intent = getIntent();
        if (intent != null) {
            ProfileProvider profileProvider = getProfileProviderSupplier().get();
            if (profileProvider != null) {
                Profile profile = profileProvider.getOriginalProfile();
                OtherDevicesShortcutController.handleShareTargetIntent(
                        this, intent, profile, this::finish);
                return;
            }
        }
        finish();
    }
}
