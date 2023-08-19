// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share.qrcode;

import static org.junit.Assert.assertEquals;

import android.view.View;

import androidx.test.filters.MediumTest;
import androidx.viewpager.widget.ViewPager;

import com.google.android.material.tabs.TabLayout;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;

import org.chromium.base.test.BaseActivityTestRule;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.test.util.BlankUiTestActivity;

import java.util.ArrayList;

/**
 * Tests for the {@link QrCodeDialog}.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Batch(Batch.UNIT_TESTS)
public class QrCodeDialogTest {
    @Rule
    public final BaseActivityTestRule<BlankUiTestActivity> mActivityTestRule =
            new BaseActivityTestRule<>(BlankUiTestActivity.class);

    public static class CustomQrCodeDialog extends QrCodeDialog {
        public void setTabs(ArrayList<QrCodeDialogTab> tabs) {
            mTabs = tabs;
        }

        @Override
        public void setWindowAndroid(WindowAndroid windowAndroid) {}
    }

    public static class CustomQrCodeDialogTab implements QrCodeDialogTab {
        private View mView;
        private boolean mEnabled;

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
        MockitoAnnotations.initMocks(this);
        mActivityTestRule.setFinishActivity(true);
    }

    @Test
    @MediumTest
    public void testGetDialogView_disabledTabIsNotInDialog() {
        mActivityTestRule.launchActivity(null);

        CustomQrCodeDialog qrCodeDialog = new CustomQrCodeDialog();
        ArrayList<QrCodeDialogTab> tabs = new ArrayList<>();
        View firstTabView = Mockito.mock(View.class);
        View secondTabView = Mockito.mock(View.class);
        tabs.add(new CustomQrCodeDialogTab(firstTabView, true));
        tabs.add(new CustomQrCodeDialogTab(secondTabView, false));
        qrCodeDialog.setTabs(tabs);

        View dialog = qrCodeDialog.getDialogView(mActivityTestRule.getActivity());
        ViewPager viewPager = dialog.findViewById(org.chromium.chrome.R.id.qrcode_view_pager);
        TabLayout tabLayout = dialog.findViewById(org.chromium.chrome.R.id.tab_layout);

        assertEquals("Only the enabled tab view should be in the viewPager.", 1,
                viewPager.getAdapter().getCount());
        assertEquals(
                "Only the enabled tab should be in the tabLayout.", 1, tabLayout.getTabCount());
    }

    @Test
    @MediumTest
    public void testGetDialogView_allTabsEnabled() {
        mActivityTestRule.launchActivity(null);

        CustomQrCodeDialog qrCodeDialog = new CustomQrCodeDialog();
        ArrayList<QrCodeDialogTab> tabs = new ArrayList<>();
        View firstTabView = Mockito.mock(View.class);
        View secondTabView = Mockito.mock(View.class);
        tabs.add(new CustomQrCodeDialogTab(firstTabView, true));
        tabs.add(new CustomQrCodeDialogTab(secondTabView, true));
        qrCodeDialog.setTabs(tabs);

        View dialog = qrCodeDialog.getDialogView(mActivityTestRule.getActivity());
        ViewPager viewPager = dialog.findViewById(org.chromium.chrome.R.id.qrcode_view_pager);
        TabLayout tabLayout = dialog.findViewById(org.chromium.chrome.R.id.tab_layout);

        assertEquals(
                "All tab views should be in the viewPager.", 2, viewPager.getAdapter().getCount());
        assertEquals("All tabs should be in the tabLayout.", 2, tabLayout.getTabCount());
    }
}
