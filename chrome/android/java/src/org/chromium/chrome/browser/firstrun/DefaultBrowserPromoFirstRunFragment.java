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

@NullMarked
public class DefaultBrowserPromoFirstRunFragment extends Fragment implements FirstRunFragment {

    @Override
    public View onCreateView(
            LayoutInflater inflater,
            @Nullable ViewGroup container,
            @Nullable Bundle savedInstanceState) {

        DefaultBrowserPromoFirstRunView view =
                (DefaultBrowserPromoFirstRunView)
                        inflater.inflate(
                                R.layout.default_browser_promo_fre_portrait_view, container, false);

        // These are just placeholders for the primer. Will upload a follow-up CL for the actual
        // logic.
        var pageDelegate = assumeNonNull(getPageDelegate());
        view.getContinueButtonView().setOnClickListener(v -> pageDelegate.advanceToNextPage());
        view.getDismissButtonView().setOnClickListener(v -> pageDelegate.advanceToNextPage());

        return view;
    }

    /** Implements {@link FirstRunFragment}. */
    @Override
    public void setInitialA11yFocus() {
        if (getView() == null) return;
        getView()
                .findViewById(R.id.title)
                .sendAccessibilityEvent(AccessibilityEvent.TYPE_VIEW_FOCUSED);
    }
}
