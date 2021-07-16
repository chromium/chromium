// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.signin;

import android.content.Context;
import android.os.Bundle;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;

import androidx.fragment.app.Fragment;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.firstrun.FirstRunFragment;
import org.chromium.chrome.browser.signin.ui.frebottomgroup.FREBottomGroupCoordinator;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogManagerHolder;

/**
 * This fragment handles the sign-in without sync consent during the FRE.
 */
public class SigninFirstRunFragment
        extends Fragment implements FirstRunFragment, FREBottomGroupCoordinator.Listener {
    private ModalDialogManager mModalDialogManager;
    private FREBottomGroupCoordinator mFREBottomGroupCoordinator;

    public SigninFirstRunFragment() {}

    @Override
    public void onAttach(Context context) {
        super.onAttach(context);
        mModalDialogManager = ((ModalDialogManagerHolder) getActivity()).getModalDialogManager();
    }

    @Override
    public View onCreateView(
            LayoutInflater inflater, ViewGroup container, Bundle savedInstanceState) {
        final View view = inflater.inflate(R.layout.signin_first_run_view, container, false);
        mFREBottomGroupCoordinator = new FREBottomGroupCoordinator(requireContext(),
                view.findViewById(R.id.signin_fre_bottom_group), mModalDialogManager, this);
        return view;
    }

    @Override
    public void onDestroy() {
        super.onDestroy();
        mFREBottomGroupCoordinator.destroy();
    }

    /**
     * Implements {@link FirstRunFragment}.
     */
    @Override
    public void setInitialA11yFocus() {}

    /**
     * Implements {@link FREBottomGroupCoordinator.Listener}.
     * TODO(crbug/1227319): Implement account addition.
     */
    @Override
    public void addAccount() {}

    /**
     * Implements {@link FREBottomGroupCoordinator.Listener}.
     */
    @Override
    public void advanceToNextPage() {
        getPageDelegate().acceptTermsOfService(true);
    }
}
