// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs.features.minimizedcustomtab;

import static org.chromium.chrome.browser.customtabs.features.minimizedcustomtab.MinimizedCardProperties.ALL_KEYS;
import static org.chromium.chrome.browser.customtabs.features.minimizedcustomtab.MinimizedCardProperties.FAVICON;
import static org.chromium.chrome.browser.customtabs.features.minimizedcustomtab.MinimizedCardProperties.TITLE;
import static org.chromium.chrome.browser.customtabs.features.minimizedcustomtab.MinimizedCardProperties.URL;

import android.os.Bundle;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.fragment.app.DialogFragment;

import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/**
 * Class that manages the showing and hiding of the fullscreen dialog that will host the minimized
 * card. It acts as a coordinator for the minimized card.
 */
public class MinimizedCardDialogFragment extends DialogFragment {
    static final String TAG = "MinimizedCardDialogFragment";
    static final String COORDINATOR_IMPORTANT_FOR_ACCESSIBILITY_KEY =
            "CoordinatorImportantForAccessibility";
    private PropertyModel mModel;

    static MinimizedCardDialogFragment newInstance(PropertyModel model) {
        var fragment = new MinimizedCardDialogFragment();
        fragment.setArguments(toArgs(model));

        return fragment;
    }

    @Override
    public void onCreate(@Nullable Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        mModel = toModel(getArguments());
    }

    @Nullable
    @Override
    public View onCreateView(@NonNull LayoutInflater inflater, @Nullable ViewGroup container,
            @Nullable Bundle savedInstanceState) {
        return inflater.inflate(
                org.chromium.chrome.R.layout.custom_tabs_minimized_card, container, false);
    }

    @Override
    public void onViewCreated(@NonNull View view, @Nullable Bundle savedInstanceState) {
        super.onViewCreated(view, savedInstanceState);

        PropertyModelChangeProcessor.create(mModel, view, MinimizedCardViewBinder::bind, true);
        View coordinator = getActivity().findViewById(org.chromium.chrome.R.id.coordinator);
        int important = coordinator.getImportantForAccessibility();
        getArguments().putInt(COORDINATOR_IMPORTANT_FOR_ACCESSIBILITY_KEY, important);
        coordinator.setImportantForAccessibility(
                View.IMPORTANT_FOR_ACCESSIBILITY_NO_HIDE_DESCENDANTS);
    }

    @Override
    public void onDestroyView() {
        int important = getArguments().getInt(COORDINATOR_IMPORTANT_FOR_ACCESSIBILITY_KEY);
        View coordinator = getActivity().findViewById(org.chromium.chrome.R.id.coordinator);
        coordinator.setImportantForAccessibility(important);

        super.onDestroyView();
    }

    private static Bundle toArgs(PropertyModel model) {
        Bundle args = new Bundle();
        args.putString(TITLE.toString(), model.get(TITLE));
        args.putString(URL.toString(), model.get(URL));
        args.putParcelable(FAVICON.toString(), model.get(FAVICON));

        return args;
    }

    private static PropertyModel toModel(Bundle bundle) {
        return new PropertyModel.Builder(ALL_KEYS)
                .with(TITLE, bundle.getString(TITLE.toString()))
                .with(URL, bundle.getString(URL.toString()))
                .with(FAVICON, bundle.getParcelable(FAVICON.toString()))
                .build();
    }
}
