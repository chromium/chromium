// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.firstrun;

import android.os.Bundle;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.view.accessibility.AccessibilityEvent;

import androidx.fragment.app.Fragment;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.ui.signin.history_sync.HistorySyncCoordinator;

public class HistorySyncFirstRunFragment extends Fragment
        implements FirstRunFragment, HistorySyncCoordinator.HistorySyncDelegate {
    private HistorySyncCoordinator mHistorySyncCoordinator;

    @Override
    public View onCreateView(
            LayoutInflater inflater, ViewGroup container, Bundle savedInstanceState) {
        assert getPageDelegate().getProfileProviderSupplier().get() != null;
        Profile profile = getPageDelegate().getProfileProviderSupplier().get().getOriginalProfile();
        mHistorySyncCoordinator = new HistorySyncCoordinator(inflater, container, this, profile);

        return mHistorySyncCoordinator.getView();
    }

    /** Implements {@link FirstRunFragment}. */
    @Override
    public void setInitialA11yFocus() {
        // Ignore calls before view is created.
        if (getView() == null) return;

        final View title = getView().findViewById(R.id.sync_consent_title);
        title.sendAccessibilityEvent(AccessibilityEvent.TYPE_VIEW_FOCUSED);
    }

    /** Implements {@link HistorySyncDelegate} */
    @Override
    public void dismiss() {
        getPageDelegate().advanceToNextPage();
    }
}
