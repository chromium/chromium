// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.devui;

import android.annotation.SuppressLint;
import android.app.Activity;
import android.os.Bundle;
import android.view.Menu;
import android.view.MenuItem;
import android.view.View;
import android.view.ViewGroup;
import android.widget.AdapterView;
import android.widget.ArrayAdapter;
import android.widget.ListView;
import android.widget.Spinner;
import android.widget.TextView;

import org.chromium.android_webview.common.Flag;
import org.chromium.android_webview.common.ProductionSupportedFlagList;
import org.chromium.android_webview.devui.util.NavigationMenuHelper;

import java.util.Arrays;
import java.util.HashSet;
import java.util.List;
import java.util.Set;

/**
 * An activity to toggle experimental WebView flags/features.
 */
@SuppressLint("SetTextI18n")
public class FlagsActivity extends Activity {
    // TODO(ntfschr): at the moment we're only writing to these sets. When we implement the service,
    // we'll also read the contents to know what to send to embedded WebViews.
    private final Set<Flag> mEnabledFlags = new HashSet<>();
    private final Set<Flag> mDisabledFlags = new HashSet<>();

    private static final String[] sFlagStates = {
            "Default",
            "Enabled",
            "Disabled",
    };

    private WebViewPackageError mDifferentPackageError;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        setContentView(R.layout.activity_flags);

        ListView flagsListView = findViewById(R.id.flags_list);
        TextView flagsDescriptionView = findViewById(R.id.flags_description);
        flagsDescriptionView.setText("By enabling these features, you could "
                + "lose app data or compromise your security or privacy. Enabled features apply to "
                + "WebViews across all apps on the device.");

        List<Flag> flagsList = Arrays.asList(ProductionSupportedFlagList.sFlagList);
        flagsListView.setAdapter(new FlagsListAdapter(flagsList));

        mDifferentPackageError =
                new WebViewPackageError(this, findViewById(R.id.flags_activity_layout));
        // show the dialog once when the activity is created.
        mDifferentPackageError.showDialogIfDifferent();
    }

    @Override
    protected void onResume() {
        super.onResume();
        // Check package status in onResume() to hide/show the error message if the user
        // changes WebView implementation from system settings and then returns back to the
        // activity.
        mDifferentPackageError.showMessageIfDifferent();
    }

    private class FlagStateSpinnerSelectedListener implements AdapterView.OnItemSelectedListener {
        private Flag mFlag;

        FlagStateSpinnerSelectedListener(Flag flag) {
            mFlag = flag;
        }

        @Override
        public void onItemSelected(AdapterView<?> parent, View view, int position, long id) {
            switch (sFlagStates[position]) {
                case "Default":
                    mEnabledFlags.remove(mFlag);
                    mDisabledFlags.remove(mFlag);
                    break;
                case "Enabled":
                    mEnabledFlags.add(mFlag);
                    mDisabledFlags.remove(mFlag);
                    break;
                case "Disabled":
                    mEnabledFlags.remove(mFlag);
                    mDisabledFlags.add(mFlag);
                    break;
            }

            // TODO(ntfschr): enable/disable enable) developer mode (when that's supported), based
            // on whether (mEnabledFlags.isEmpty() && mDisabledFlags.isEmpty()).
        }

        @Override
        public void onNothingSelected(AdapterView<?> parent) {}
    }

    /**
     * Adapter to create rows of toggleable Flags.
     */
    private class FlagsListAdapter extends ArrayAdapter<Flag> {
        private final List<Flag> mFlagsList;

        public FlagsListAdapter(List<Flag> flagsList) {
            super(FlagsActivity.this, R.layout.toggleable_flag, flagsList);
            mFlagsList = flagsList;
        }

        @Override
        public View getView(int position, View view, ViewGroup parent) {
            // If the the old view is already created then reuse it, else create a new one by layout
            // inflation.
            if (view == null) {
                view = getLayoutInflater().inflate(R.layout.toggleable_flag, null);
            }

            TextView flagName = view.findViewById(R.id.flag_name);
            TextView flagDescription = view.findViewById(R.id.flag_description);
            Spinner flagToggle = view.findViewById(R.id.flag_toggle);

            Flag flag = getItem(position);
            flagName.setText(flag.getName());
            flagDescription.setText(flag.getDescription());
            ArrayAdapter<String> adapter =
                    new ArrayAdapter<>(FlagsActivity.this, R.layout.flag_states, sFlagStates);
            adapter.setDropDownViewResource(android.R.layout.select_dialog_singlechoice);
            flagToggle.setAdapter(adapter);
            flagToggle.setOnItemSelectedListener(new FlagStateSpinnerSelectedListener(flag));

            return view;
        }
    }

    @Override
    public boolean onCreateOptionsMenu(Menu menu) {
        NavigationMenuHelper.inflate(this, menu);
        return true;
    }

    @Override
    public boolean onOptionsItemSelected(MenuItem item) {
        if (NavigationMenuHelper.onOptionsItemSelected(this, item)) {
            return true;
        }
        return super.onOptionsItemSelected(item);
    }
}
