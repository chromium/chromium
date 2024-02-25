// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.devui;

import android.app.Activity;
import android.content.Context;
import android.os.Bundle;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ArrayAdapter;
import android.widget.ListView;
import android.widget.TextView;

import org.chromium.android_webview.devui.util.SafeModeActionInfo;
import org.chromium.android_webview.devui.util.SafeModeInfo;

import java.util.Date;
import java.util.List;
import java.util.Locale;
import java.util.stream.Collectors;

/** A fragment to show WebView SafeMode status. */
public class SafeModeFragment extends DevUiBaseFragment {
    private Context mContext;
    private TextView mSafeModeState;
    private ListView mActionsListView;
    private View mActionsContainer;

    @Override
    public void onAttach(Context context) {
        super.onAttach(context);
        mContext = context;
    }

    @Override
    public View onCreateView(
            LayoutInflater inflater, ViewGroup container, Bundle savedInstanceState) {
        return inflater.inflate(R.layout.fragment_safe_mode, null);
    }

    @Override
    public void onViewCreated(View view, Bundle savedInstanceState) {
        Activity activity = (Activity) mContext;
        activity.setTitle("WebView SafeMode");

        mSafeModeState = view.findViewById(R.id.safe_mode_state);
        mActionsListView = view.findViewById(R.id.safe_mode_actions_list);
        mActionsContainer = view.findViewById(R.id.safe_mode_actions_container);
    }

    @Override
    public void onResume() {
        super.onResume();

        SafeModeInfo safeModeInfo = new SafeModeInfo(mContext, mContext.getPackageName());
        boolean safeModeEnabled = safeModeInfo.isEnabledForUI();
        mSafeModeState.setText(safeModeEnabled ? "Enabled" : "Disabled");
        mActionsContainer.setVisibility(safeModeEnabled ? View.VISIBLE : View.INVISIBLE);

        if (safeModeEnabled) {
            safeModeInfo.getActivationTimeForUI(
                    activationTime -> {
                        mSafeModeState.setText(
                                String.format(
                                        Locale.US, "Enabled on %s", new Date(activationTime)));
                    });

            List<SafeModeActionInfo> actions =
                    safeModeInfo.getActionsForUI().stream()
                            .map(SafeModeActionInfo::new)
                            .collect(Collectors.toList());
            mActionsListView.setAdapter(new ActionsListAdapter(actions));
        }
    }

    private class ActionsListAdapter extends ArrayAdapter<SafeModeActionInfo> {
        public ActionsListAdapter(List<SafeModeActionInfo> actions) {
            super(mContext, R.layout.safe_mode_actions_list_item, actions);
        }

        @Override
        public View getView(int position, View view, ViewGroup parent) {
            // If the the old view is already created then reuse it, else create a new one by layout
            // inflation.
            if (view == null) {
                view =
                        getLayoutInflater()
                                .inflate(R.layout.safe_mode_actions_list_item, null, true);
            }
            SafeModeActionInfo item = getItem(position);
            TextView actionId = view.findViewById(R.id.action_id_textview);
            actionId.setText(item.getId());
            return view;
        }
    }
}
