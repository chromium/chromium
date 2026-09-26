// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share.qrcode;

import static org.junit.Assert.assertEquals;

import android.app.Activity;
import android.view.View;

import androidx.viewpager.widget.ViewPager;

import com.google.android.material.tabs.TabLayout;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.R;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.base.WindowAndroid;

import java.util.ArrayList;

/** Tests for the {@link QrCodeDialog}. */
@RunWith(BaseRobolectricTestRunner.class)
public class QrCodeDialogTest {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private View mTabView;

    private Activity mActivity;

    private static class CustomQrCodeDialog extends QrCodeDialog {
        private void setTabs(ArrayList<QrCodeDialogTab> tabs) {
            mTabs = tabs;
        }

        @Override
        public void setWindowAndroid(WindowAndroid windowAndroid) {}
    }

    private static class CustomQrCodeDialogTab implements QrCodeDialogTab {
        private final View mView;
        private final boolean mEnabled;

        CustomQrCodeDialogTab(View view, boolean enabled) {
            mView = view;
            mEnabled = enabled;
        }

        @Override
        public View getView() {
            return mView;
        }

        @Override
        public boolean isEnabled() {
            return mEnabled;
        }

        @Override
        public void onResume() {}

        @Override
        public void onPause() {}

        @Override
        public void onDestroy() {}

        @Override
        public void updatePermissions(WindowAndroid windowAndroid) {}
    }

    @Before
    public void setUp() {
        mActivity = Robolectric.buildActivity(TestActivity.class).setup().get();
    }

    @Test
    public void testGetDialogView_disabledTabIsNotInDialog() {
        CustomQrCodeDialog qrCodeDialog = new CustomQrCodeDialog();
        ArrayList<QrCodeDialogTab> tabs = new ArrayList<>();
        tabs.add(new CustomQrCodeDialogTab(mTabView, false));
        qrCodeDialog.setTabs(tabs);

        View dialog = qrCodeDialog.getDialogView(mActivity);
        ViewPager viewPager = dialog.findViewById(R.id.qrcode_view_pager);
        TabLayout tabLayout = dialog.findViewById(R.id.tab_layout);

        assertEquals(
                "Tab is disabled and should not be in adapter.",
                0,
                viewPager.getAdapter().getCount());
        assertEquals(
                "Tab is disabled and should not be in tab layout.", 0, tabLayout.getTabCount());
    }

    @Test
    public void testGetDialogView() {
        CustomQrCodeDialog qrCodeDialog = new CustomQrCodeDialog();
        ArrayList<QrCodeDialogTab> tabs = new ArrayList<>();
        tabs.add(new CustomQrCodeDialogTab(mTabView, true));
        qrCodeDialog.setTabs(tabs);

        View dialog = qrCodeDialog.getDialogView(mActivity);
        ViewPager viewPager = dialog.findViewById(R.id.qrcode_view_pager);
        TabLayout tabLayout = dialog.findViewById(R.id.tab_layout);

        assertEquals("Tab views should be in the viewPager.", 1, viewPager.getAdapter().getCount());
        assertEquals("Tabs should be in the tabLayout.", 1, tabLayout.getTabCount());
    }
}
