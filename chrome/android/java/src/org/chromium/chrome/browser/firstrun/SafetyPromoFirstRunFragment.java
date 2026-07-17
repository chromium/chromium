// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.firstrun;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.os.Bundle;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.view.accessibility.AccessibilityEvent;

import androidx.fragment.app.Fragment;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;

/** A {@link Fragment} for the Safety Promo during the First Run Experience (FRE). */
@NullMarked
public class SafetyPromoFirstRunFragment extends Fragment implements FirstRunFragment {
    @Override
    public View onCreateView(
            LayoutInflater inflater,
            @Nullable ViewGroup container,
            @Nullable Bundle savedInstanceState) {
        SafetyPromoFirstRunView view =
                (SafetyPromoFirstRunView)
                        inflater.inflate(
                                R.layout.safety_promo_fre_illustration_portrait_view,
                                container,
                                false);

        // These are just placeholders for the arm 4 UI skeleton. Actual illustration and animation
        // logic will be added in follow-up patches.
        var pageDelegate = assumeNonNull(getPageDelegate());
        view.getContinueButtonView().setOnClickListener(v -> pageDelegate.advanceToNextPage());

        return view;
    }

    @Override
    public void setInitialA11yFocus() {
        if (getView() == null) return;

        getView()
                .findViewById(R.id.title)
                .sendAccessibilityEvent(AccessibilityEvent.TYPE_VIEW_FOCUSED);
    }
}
