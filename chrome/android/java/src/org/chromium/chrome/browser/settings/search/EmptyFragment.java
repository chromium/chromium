// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.settings.search;

import android.app.Activity;
import android.content.Context;
import android.os.Bundle;
import android.text.SpannableString;
import android.text.Spanned;
import android.text.style.ForegroundColorSpan;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ImageView;
import android.widget.TextView;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.fragment.app.Fragment;

import org.chromium.build.annotations.EnsuresNonNull;
import org.chromium.build.annotations.Initializer;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.R;
import org.chromium.components.browser_ui.styles.SemanticColorUtils;

/** Empty Fragment used to clear the settings screen and display guiding information. */
@NullMarked
public class EmptyFragment extends Fragment {

    private int mImageSrc;
    private Runnable mOpenHelpCenter;

    @Initializer
    @EnsuresNonNull("mOpenHelpCenter")
    public void init(int imageSrc, Runnable openHelpCenter) {
        mImageSrc = imageSrc;
        mOpenHelpCenter = openHelpCenter;
    }

    @Override
    public @Nullable View onCreateView(
            LayoutInflater inflater,
            @Nullable ViewGroup container,
            @Nullable Bundle savedInstanceState) {
        return inflater.inflate(R.layout.empty_state_view, container, false);
    }

    @Override
    public void onViewCreated(@NonNull View view, @Nullable Bundle savedInstanceState) {
        super.onViewCreated(view, savedInstanceState);
        if (mImageSrc == 0) {
            clear();
            return;
        }
        ImageView stateImage = view.findViewById(R.id.empty_state_icon);
        stateImage.setImageResource(mImageSrc);

        TextView guideMsg = view.findViewById(R.id.empty_state_text_title);
        TextView linkHelpCenter = view.findViewById(R.id.empty_state_text_description);
        int guideMsgId = R.string.search_in_settings_zero_state;
        int linkVisibility = View.GONE;
        Context context = view.getContext();
        if (mImageSrc == R.drawable.settings_no_match) {
            guideMsgId = R.string.search_in_settings_no_match;
            linkVisibility = View.VISIBLE;
            linkHelpCenter.setText(getHelpCenterString(context));
            linkHelpCenter.setOnClickListener(v -> mOpenHelpCenter.run());
            linkHelpCenter.setFocusable(true);
            linkHelpCenter.setClickable(true);
        } else {
            assert mImageSrc == R.drawable.settings_zero_state
                    : "Invalid fragment resource: " + mImageSrc;
        }
        guideMsg.setText(context.getString(guideMsgId));
        linkHelpCenter.setVisibility(linkVisibility);
    }

    private static SpannableString getHelpCenterString(Context context) {
        String helpCenter = context.getString(R.string.search_in_settings_help_center);
        var ss = new SpannableString(helpCenter);
        var fcs = new ForegroundColorSpan(SemanticColorUtils.getDefaultTextColorLink(context));
        ss.setSpan(fcs, 0, helpCenter.length(), Spanned.SPAN_EXCLUSIVE_EXCLUSIVE);
        return ss;
    }

    void clear() {
        // No need to actively hide the widgets when the fragment is going away.
        if (isRemoving()) return;

        Activity activity = getActivity();
        activity.findViewById(R.id.empty_state_icon).setVisibility(View.GONE);
        activity.findViewById(R.id.empty_state_text_title).setVisibility(View.GONE);
        activity.findViewById(R.id.empty_state_text_description).setVisibility(View.GONE);
    }
}
