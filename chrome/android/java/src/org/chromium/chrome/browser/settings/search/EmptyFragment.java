// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.settings.search;

import android.app.Activity;
import android.content.Context;
import android.os.Bundle;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;
import android.widget.ImageView;
import android.widget.TextView;

import androidx.fragment.app.Fragment;
import androidx.window.layout.WindowMetricsCalculator;

import org.chromium.build.annotations.EnsuresNonNull;
import org.chromium.build.annotations.Initializer;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.ui.widget.ButtonCompat;

/** Empty Fragment used to clear the settings screen and display guiding information. */
@NullMarked
public class EmptyFragment extends Fragment {
    private static final String KEY_IMAGE_SRC = "ImageSrc";

    private int mImageSrc;
    private Runnable mOpenHelpCenter;

    @Initializer
    public void setImageSrc(int imageSrc) {
        mImageSrc = imageSrc;
    }

    @Initializer
    @EnsuresNonNull("mOpenHelpCenter")
    public void setOpenHelpCenter(Runnable openHelpCenter) {
        mOpenHelpCenter = openHelpCenter;
    }

    @Override
    public @Nullable View onCreateView(
            LayoutInflater inflater,
            @Nullable ViewGroup container,
            @Nullable Bundle savedInstanceState) {
        return inflater.inflate(R.layout.settings_empty_state_view, container, false);
    }

    @Override
    public void onViewCreated(View view, @Nullable Bundle savedInstanceState) {
        super.onViewCreated(view, savedInstanceState);
        if (savedInstanceState != null) mImageSrc = savedInstanceState.getInt(KEY_IMAGE_SRC);
        if (mImageSrc == 0) {
            clear();
            return;
        }
        ImageView stateImage = view.findViewById(R.id.empty_state_icon);
        stateImage.setImageResource(mImageSrc);
        View content = view.findViewById(R.id.empty_state_content);
        var lp = (FrameLayout.LayoutParams) content.getLayoutParams();
        var windowMetrics =
                WindowMetricsCalculator.getOrCreate().computeCurrentWindowMetrics(getActivity());

        // Position the content above the center so that they remain visible with the soft
        // keyboard likely also visible on screen most of the time.
        lp.topMargin = windowMetrics.getBounds().height() / 6;
        content.setLayoutParams(lp);

        TextView guideMsg = view.findViewById(R.id.empty_state_text_title);
        ButtonCompat linkHelpCenter = view.findViewById(R.id.empty_state_text_description);
        int guideMsgId = R.string.search_in_settings_zero_state;
        int linkVisibility = View.GONE;
        Context context = view.getContext();
        if (mImageSrc == R.drawable.settings_no_match) {
            guideMsgId = R.string.search_in_settings_no_match;
            linkVisibility = View.VISIBLE;
            linkHelpCenter.setText(context.getString(R.string.search_in_settings_help_center));
            linkHelpCenter.setOnClickListener(v -> mOpenHelpCenter.run());
        } else {
            assert mImageSrc == R.drawable.settings_zero_state
                    : "Invalid fragment resource: " + mImageSrc;
        }
        guideMsg.setText(context.getString(guideMsgId));
        linkHelpCenter.setVisibility(linkVisibility);
    }

    @Override
    public void onSaveInstanceState(Bundle outState) {
        super.onSaveInstanceState(outState);
        outState.putInt(KEY_IMAGE_SRC, mImageSrc);
    }

    void clear() {
        // No need to actively hide the widgets when the fragment is going away.
        Activity activity = getActivity();
        if (isRemoving() || activity.isDestroyed()) return;

        activity.findViewById(R.id.empty_state_icon).setVisibility(View.GONE);
        activity.findViewById(R.id.empty_state_text_title).setVisibility(View.GONE);
        activity.findViewById(R.id.empty_state_text_description).setVisibility(View.GONE);
    }
}
