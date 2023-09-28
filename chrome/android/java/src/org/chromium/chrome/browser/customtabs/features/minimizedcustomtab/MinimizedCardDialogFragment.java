// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs.features.minimizedcustomtab;

import android.os.Bundle;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.fragment.app.DialogFragment;

/**
 * Class that manages the showing and hiding of the fullscreen dialog that will host the minimized
 * card.
 */
public class MinimizedCardDialogFragment extends DialogFragment {
    private final View mMinimizedCardView;

    MinimizedCardDialogFragment(View minimizedCardView) {
        mMinimizedCardView = minimizedCardView;
    }

    @Nullable
    @Override
    public View onCreateView(@NonNull LayoutInflater inflater, @Nullable ViewGroup container,
            @Nullable Bundle savedInstanceState) {
        return mMinimizedCardView;
    }
}
