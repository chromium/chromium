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
import org.chromium.chrome.browser.ui.signin.history_sync.HistorySyncView;
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
        createViewAndAttachToFragment();
    }

    @Override
    public void onConfigurationChanged(Configuration newConfig) {
        super.onConfigurationChanged(newConfig);
        createViewAndAttachToFragment();
    }

    private void createViewAndAttachToFragment() {
        maybeCreateCoordinator();
        HistorySyncView view = mHistorySyncCoordinator.maybeRecreateView();
        if (view != null) {
            // View is non-null when HistorySyncView has created a new view. This new view will
            // replace any pre-existing view.
            mFragmentView.removeAllViews();
            mFragmentView.addView(mHistorySyncCoordinator.getView());
        }
    }

    private void maybeCreateCoordinator() {
        if (mHistorySyncCoordinator != null) return;

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
                        getActivity(),
                        this,
                        profile,
                        SigninAccessPoint.START_PAGE,
                        false,
                        false,
                        null);
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

    /** Implements {@link HistorySyncDelegate} */
    @Override
    public void maybeRecordFreProgress(@MobileFreProgress int state) {
        getPageDelegate().recordFreProgressHistogram(state);
    }

    @Override
    public void onDetach() {
        super.onDetach();
        if (mHistorySyncCoordinator != null) {
            mHistorySyncCoordinator.destroy();
        }
    }
}
