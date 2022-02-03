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
 * Dialog shown for the Privacy Sandbox.
 */
public class PrivacySandboxDialog extends Dialog {
    public PrivacySandboxDialog(Context context) {
        super(context, R.style.ThemeOverlay_BrowserUI_Fullscreen);
        View view = LayoutInflater.from(context).inflate(R.layout.privacy_sandbox_dialog, null);
        setContentView(view);

        // TODO(crbug.com/1286276): Add the actual logic for the buttons.
        ButtonCompat yesButton = (ButtonCompat) view.findViewById(R.id.yes_button);
        yesButton.setOnClickListener((View v) -> dismiss());
        ButtonCompat noButton = (ButtonCompat) view.findViewById(R.id.no_button);
        noButton.setOnClickListener((View v) -> dismiss());
    }
}
