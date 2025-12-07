// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.privacy_guide;

import android.os.Bundle;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

/** First privacy guide page. */
@NullMarked
public class WelcomeFragment extends PrivacyGuideBasePage {
    @Override
    public View onCreateView(
            LayoutInflater inflater,
            @Nullable ViewGroup container,
            @Nullable Bundle savedInstanceState) {
        return inflater.inflate(R.layout.privacy_guide_welcome, container, false);
    }

    @Override
    public void onViewCreated(View view, @Nullable Bundle savedInstanceState) {
        super.onViewCreated(view, savedInstanceState);
        // ScrollView sets focusable to true during construction. So setting focusable to false in
        // xml file doesn't work. It has to be set after the construction of ScrollView.
        view.findViewById(R.id.privacy_guide_welcome_scrollview).setFocusable(false);
    }
}
