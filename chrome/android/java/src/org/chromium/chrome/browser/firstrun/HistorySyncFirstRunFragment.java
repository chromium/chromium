// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.firstrun;

import android.content.res.Configuration;
import android.os.Bundle;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.view.accessibility.AccessibilityEvent;
import android.widget.FrameLayout;

import androidx.fragment.app.Fragment;

import org.chromium.base.Log;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.ui.signin.history_sync.HistorySyncCoordinator;
import org.chromium.components.signin.identitymanager.ConsentLevel;
import org.chromium.components.signin.metrics.SigninAccessPoint;

public class HistorySyncFirstRunFragment extends Fragment
        implements FirstRunFragment, HistorySyncCoordinator.HistorySyncDelegate {
    private static final String TAG = "HistorySyncFREFrag";

    private HistorySyncCoordinator mHistorySyncCoordinator;
    private FrameLayout mFragmentView;

    @Override
    public View onCreateView(
            LayoutInflater inflater, ViewGroup container, Bundle savedInstanceState) {
        mFragmentView = new FrameLayout(getActivity());

        return mFragmentView;
    }

    @Override
    public void onResume() {
        super.onResume();
        createCoordinatorAndAddToFragment();
    }

    @Override
    public void onPause() {
        super.onPause();
        if (mHistorySyncCoordinator != null) {
            mHistorySyncCoordinator.destroy();
            mHistorySyncCoordinator = null;
        }
    }

    @Override
    public void onConfigurationChanged(Configuration newConfig) {
        super.onConfigurationChanged(newConfig);
        if (mHistorySyncCoordinator == null) {
            return;
        }
        createCoordinatorAndAddToFragment();
    }

    /** Implements {@link FirstRunFragment}. */
    @Override
    public void setInitialA11yFocus() {
        // Ignore calls before view is created.
        if (getView() == null || mHistorySyncCoordinator == null) return;

        final View title = getView().findViewById(R.id.history_sync_title);
        title.sendAccessibilityEvent(AccessibilityEvent.TYPE_VIEW_FOCUSED);
    }

    /** Implements {@link HistorySyncDelegate} */
    @Override
    public void dismissHistorySync() {
        getPageDelegate().advanceToNextPage();
        if (mHistorySyncCoordinator != null) {
            mHistorySyncCoordinator.destroy();
            mHistorySyncCoordinator = null;
        }
    }

    @Override
    public boolean isLargeScreen() {
        return !getPageDelegate().canUseLandscapeLayout();
    }

    private void createCoordinatorAndAddToFragment() {
        if (mHistorySyncCoordinator != null) {
            mHistorySyncCoordinator.destroy();
        }
        assert getPageDelegate().getProfileProviderSupplier().get() != null;
        Profile profile = getPageDelegate().getProfileProviderSupplier().get().getOriginalProfile();
        if (IdentityServicesProvider.get()
                        .getSigninManager(profile)
                        .getIdentityManager()
                        .getPrimaryAccountInfo(ConsentLevel.SIGNIN)
                == null) {
            Log.w(TAG, "No primary account set, dismissing the history sync screen.");
            getPageDelegate().advanceToNextPage();
            return;
        }
        mHistorySyncCoordinator =
                new HistorySyncCoordinator(
                        getContext(),
                        this,
                        profile,
                        SigninAccessPoint.START_PAGE,
                        false,
                        false,
                        null);
        mFragmentView.removeAllViews();
        mFragmentView.addView(mHistorySyncCoordinator.getView());
    }
}
