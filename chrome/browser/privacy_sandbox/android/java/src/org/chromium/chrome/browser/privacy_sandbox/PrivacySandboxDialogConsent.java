// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.privacy_sandbox;

import android.app.Dialog;
import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;

import org.chromium.ui.widget.ButtonCompat;

/**
 * Dialog in the form of a consent shown for the Privacy Sandbox.
 */
public class PrivacySandboxDialogConsent extends Dialog implements View.OnClickListener {
    public PrivacySandboxDialogConsent(Context context) {
        super(context, R.style.ThemeOverlay_BrowserUI_Fullscreen);
        View view = LayoutInflater.from(context).inflate(R.layout.privacy_sandbox_consent, null);
        setContentView(view);

        ButtonCompat yesButton = (ButtonCompat) view.findViewById(R.id.yes_button);
        yesButton.setOnClickListener(this);
        ButtonCompat noButton = (ButtonCompat) view.findViewById(R.id.no_button);
        noButton.setOnClickListener(this);
    }

    // OnClickListener:

    @Override
    public void onClick(View view) {
        // TODO(crbug.com/1286276): Add the actual logic for the buttons.
        dismiss();
    }
}
