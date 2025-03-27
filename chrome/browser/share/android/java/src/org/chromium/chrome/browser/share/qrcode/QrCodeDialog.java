// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share.qrcode;

import android.app.Activity;
import android.app.Dialog;
import android.app.DialogFragment;
import android.content.Context;
import android.content.res.Configuration;
import android.os.Bundle;
import android.view.View;

import androidx.annotation.VisibleForTesting;
import androidx.viewpager.widget.ViewPager;

import com.google.android.material.tabs.TabLayout;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.share.qrcode.share_tab.QrCodeShareCoordinator;
import org.chromium.components.browser_ui.widget.FullscreenAlertDialog;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.widget.ChromeImageButton;

import java.util.ArrayList;

/** QrCodeDialog is the main view for QR code sharing and scanning. */
public class QrCodeDialog extends DialogFragment {
    // Used to pass the URL in the bundle.
    public static String URL_KEY = "url_key";

    private WindowAndroid mWindowAndroid;
    // TODO(crbug.com/40280300): Remove list of Tabs.
    protected ArrayList<QrCodeDialogTab> mTabs;
    private TabLayoutPageListener mTabLayoutPageListener;

    /**
     * Create a new instance of {@link QrCodeDialog} and set the URL.
     * @param windowAndroid The AndroidPermissionDelegate to be query for download permissions.
     */
    static QrCodeDialog newInstance(String url, WindowAndroid windowAndroid) {
        assert windowAndroid != null;
        QrCodeDialog qrCodeDialog = new QrCodeDialog();
        Bundle args = new Bundle();
        args.putString(URL_KEY, url);
        qrCodeDialog.setArguments(args);
        qrCodeDialog.setWindowAndroid(windowAndroid);
        return qrCodeDialog;
    }

    @Override
    public Dialog onCreateDialog(Bundle savedInstanceState) {
        initTabs();
        return new FullscreenAlertDialog.Builder(getActivity())
                .setView(getDialogView(getActivity()))
                .create();
    }

    @Override
    public void onResume() {
        super.onResume();
        mTabLayoutPageListener.resumeSelectedTab();
    }

    @Override
    public void onPause() {
        super.onPause();
        mTabLayoutPageListener.pauseAllTabs();
    }

    @Override
    public void onDestroy() {
        super.onDestroy();
        for (QrCodeDialogTab tab : mTabs) {
            tab.onDestroy();
        }
        mTabs.clear();
        mWindowAndroid = null;
    }

    @Override
    public void onConfigurationChanged(Configuration newConfig) {
        super.onConfigurationChanged(newConfig);
        // There is a corner case where this function can be triggered by toggling the battery saver
        // state, resulting in all the variables being reset. The only way out is to destroy this
        // dialog to bring the user back to the web page.
        if (mWindowAndroid == null || mTabLayoutPageListener == null) {
            onDestroyView();
        }
    }

    /**
     * Setter for the current WindowAndroid.
     * @param windowAndroid The windowAndroid to set.
     */
    public void setWindowAndroid(WindowAndroid windowAndroid) {
        mWindowAndroid = windowAndroid;
        if (mTabLayoutPageListener != null) {
            mTabLayoutPageListener.updatePermissions(mWindowAndroid);
        }
    }

    @VisibleForTesting
    protected View getDialogView(Activity activity) {
        View dialogView = activity.getLayoutInflater().inflate(R.layout.qrcode_dialog, null);
        ChromeImageButton closeButton = dialogView.findViewById(R.id.close_button);
        closeButton.setOnClickListener(v -> dismiss());
        TabLayout tabLayout = dialogView.findViewById(R.id.tab_layout);
        assert tabLayout.getTabCount() == mTabs.size();

        // Setup page adapter and tab layout.
        ArrayList<View> pages = new ArrayList<View>();

        for (int index = 0; index < mTabs.size(); index++) {
            QrCodeDialogTab tab = mTabs.get(index);
            if (tab.isEnabled()) {
                pages.add(tab.getView());
            } else {
                tabLayout.removeTabAt(index);
            }
        }
        QrCodePageAdapter pageAdapter = new QrCodePageAdapter(pages);

        ViewPager viewPager = dialogView.findViewById(R.id.qrcode_view_pager);
        viewPager.setAdapter(pageAdapter);

        mTabLayoutPageListener = new TabLayoutPageListener(tabLayout, mTabs);
        viewPager.addOnPageChangeListener(mTabLayoutPageListener);
        tabLayout.addOnTabSelectedListener(new TabLayout.ViewPagerOnTabSelectedListener(viewPager));
        return dialogView;
    }

    private void initTabs() {
        Context context = getActivity();

        QrCodeShareCoordinator shareCoordinator =
                new QrCodeShareCoordinator(
                        context, this::dismiss, getArguments().getString(URL_KEY), mWindowAndroid);

        mTabs = new ArrayList<>();
        mTabs.add(shareCoordinator);
    }
}
