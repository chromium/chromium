// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.webview_shell;

import android.os.Bundle;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.appcompat.app.ActionBar;
import androidx.appcompat.app.AppCompatActivity;
import androidx.fragment.app.FragmentManager;

public class WebViewMultiProfileBrowserActivity extends AppCompatActivity {

    private static final String PROFILE_ONE_NAME = "ProfileOne";
    private static final String PROFILE_TWO_NAME = "ProfileTwo";

    public WebViewMultiProfileBrowserActivity() {
        super(R.layout.activity_webview_multi_profile);
    }

    @Override
    protected void onCreate(@Nullable Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setSupportActionBar(findViewById(R.id.browser_toolbar));
        ActionBar actionBar = getSupportActionBar();
        assert actionBar != null;
        actionBar.setTitle(getResources().getString(R.string.title_activity_multi_profile));

        if (savedInstanceState == null) {
            final FragmentManager fm = getSupportFragmentManager();

            // Profile one browser fragment.
            final Bundle browserOneBundle = new Bundle();
            browserOneBundle.putString(WebViewBrowserFragment.ARG_PROFILE, PROFILE_ONE_NAME);
            fm.beginTransaction()
                    .setReorderingAllowed(true)
                    .add(
                            R.id.profile_one_browser_container,
                            WebViewBrowserFragment.class,
                            browserOneBundle,
                            PROFILE_ONE_NAME)
                    .commitNow();

            // Profile two browser fragment.
            final Bundle browserTwoBundle = new Bundle();
            browserTwoBundle.putString(WebViewBrowserFragment.ARG_PROFILE, PROFILE_TWO_NAME);
            fm.beginTransaction()
                    .setReorderingAllowed(true)
                    .add(
                            R.id.profile_two_browser_container,
                            WebViewBrowserFragment.class,
                            browserTwoBundle,
                            PROFILE_TWO_NAME)
                    .commitNow();

            WebViewBrowserFragment browserOneFragment =
                    (WebViewBrowserFragment) fm.findFragmentByTag(PROFILE_ONE_NAME);
            assert browserOneFragment != null;
            initBrowserFragment(browserOneFragment);

            WebViewBrowserFragment browserTwoFragment =
                    (WebViewBrowserFragment) fm.findFragmentByTag(PROFILE_TWO_NAME);
            assert browserTwoFragment != null;
            initBrowserFragment(browserTwoFragment);
        }
    }

    private void initBrowserFragment(@NonNull WebViewBrowserFragment browserFragment) {
        browserFragment.setActivityResultRegistry(getActivityResultRegistry());
    }
}
