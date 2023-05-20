// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.privacy_sandbox;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;

import androidx.annotation.NonNull;

import org.chromium.components.browser_ui.settings.SettingsLauncher;
import org.chromium.components.browser_ui.widget.ChromeDialog;
import org.chromium.ui.widget.ButtonCompat;

/**
 * Dialog in the form of a notice shown for the Privacy Sandbox.
 */
public class PrivacySandboxDialogNotice extends ChromeDialog implements View.OnClickListener {
    private Context mContext;
    private SettingsLauncher mSettingsLauncher;

    public PrivacySandboxDialogNotice(Context context, @NonNull SettingsLauncher settingsLauncher) {
        super(context, R.style.ThemeOverlay_BrowserUI_Fullscreen);
        mContext = context;
        mSettingsLauncher = settingsLauncher;
        View view = LayoutInflater.from(context).inflate(R.layout.privacy_sandbox_notice, null);
        setContentView(view);

        ButtonCompat ackButton = (ButtonCompat) view.findViewById(R.id.ack_button);
        ackButton.setOnClickListener(this);
        ButtonCompat settingsButton = (ButtonCompat) view.findViewById(R.id.settings_button);
        settingsButton.setOnClickListener(this);
    }

    @Override
    public void show() {
        PrivacySandboxBridge.promptActionOccurred(PromptAction.NOTICE_SHOWN);
        super.show();
    }

    // OnClickListener:

    @Override
    public void onClick(View view) {
        int id = view.getId();
        if (id == R.id.ack_button) {
            PrivacySandboxBridge.promptActionOccurred(PromptAction.NOTICE_ACKNOWLEDGE);
            dismiss();
        } else if (id == R.id.settings_button) {
            PrivacySandboxBridge.promptActionOccurred(PromptAction.NOTICE_OPEN_SETTINGS);
            dismiss();
            PrivacySandboxSettingsBaseFragment.launchPrivacySandboxSettings(
                    mContext, mSettingsLauncher, PrivacySandboxReferrer.PRIVACY_SANDBOX_NOTICE);
        }
    }
}
