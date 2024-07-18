// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.privacy_guide;

import android.os.Bundle;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;

import org.chromium.components.browser_ui.widget.MaterialSwitchWithText;

/** Controls the behavior of the Topics privacy guide page. */
public class AdTopicsFragment extends PrivacyGuideBasePage {
    @Override
    public View onCreateView(
            LayoutInflater inflater, ViewGroup container, Bundle savedInstanceState) {
        return inflater.inflate(R.layout.privacy_guide_ad_topics_step, container, false);
    }

    @Override
    public void onViewCreated(View view, Bundle savedInstanceState) {
        MaterialSwitchWithText adTopicsSwitch = view.findViewById(R.id.ad_topics_switch);
        // TODO (b/347054774): Replace `false` with a helper method that sets switch based on Ad
        // Topics state
        adTopicsSwitch.setChecked(false);
    }
}
