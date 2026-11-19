// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share.qrcode;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyString;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.graphics.Bitmap;
import android.os.Bundle;
import android.os.Looper;
import android.view.View;

import androidx.viewpager.widget.ViewPager;

import com.google.android.material.tabs.TabLayout;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;
import org.robolectric.Shadows;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.init.ChromeBrowserInitializer;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.share.BitmapDownloadRequest;
import org.chromium.chrome.browser.share.BitmapDownloadRequestJni;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.base.WindowAndroid;

import java.util.ArrayList;

/** Tests for the {@link QrCodeDialog}. */
@RunWith(BaseRobolectricTestRunner.class)
public class QrCodeDialogTest {
    private static final String TEST_URL = "https://example.com/";

    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private ChromeBrowserInitializer mBrowserInitializer;
    @Mock private QRCodeGenerator.Natives mQrCodeGeneratorJni;
    @Mock private BitmapDownloadRequest.Natives mBitmapDownloadRequestJni;
    @Mock private WindowAndroid mWindowAndroid;
    @Mock private Profile mProfile;

    private View mTabView;
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
        ChromeBrowserInitializer.setForTesting(mBrowserInitializer);
        doAnswer(
                        invocation -> {
                            ((Runnable) invocation.getArgument(0)).run();
                            return null;
                        })
                .when(mBrowserInitializer)
                .runNowOrAfterFullBrowserStarted(any());

        QRCodeGeneratorJni.setInstanceForTesting(mQrCodeGeneratorJni);
        Bitmap bitmap = Bitmap.createBitmap(10, 10, Bitmap.Config.ARGB_8888);
        when(mQrCodeGeneratorJni.generateBitmap(anyString())).thenReturn(bitmap);
        BitmapDownloadRequestJni.setInstanceForTesting(mBitmapDownloadRequestJni);
        when(mWindowAndroid.hasPermission(anyString())).thenReturn(true);

        mActivity = Robolectric.buildActivity(TestActivity.class).setup().get();
        mTabView = new View(mActivity);
    }

    @After
    public void tearDown() {
        ChromeBrowserInitializer.setForTesting(null);
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

    @Test
    public void testDownloadQrCode_PassesProfile() {
        QrCodeDialog qrCodeDialog = QrCodeDialog.newInstance(TEST_URL, mWindowAndroid, mProfile);
        qrCodeDialog.show(mActivity.getFragmentManager(), null);
        mActivity.getFragmentManager().executePendingTransactions();
        Shadows.shadowOf(Looper.getMainLooper()).idle();

        View downloadButton = qrCodeDialog.mTabs.get(0).getView().findViewById(R.id.download);
        assertNotNull(downloadButton);
        downloadButton.performClick();

        verify(mBitmapDownloadRequestJni)
                .downloadBitmap(eq(mProfile), anyString(), any(Bitmap.class));
    }

    @Test
    public void testDownloadQrCode_ShutdownStartedProfileSkipsDownload() {
        QrCodeDialog qrCodeDialog = QrCodeDialog.newInstance(TEST_URL, mWindowAndroid, mProfile);
        qrCodeDialog.show(mActivity.getFragmentManager(), null);
        mActivity.getFragmentManager().executePendingTransactions();
        Shadows.shadowOf(Looper.getMainLooper()).idle();

        when(mProfile.shutdownStarted()).thenReturn(true);
        View downloadButton = qrCodeDialog.mTabs.get(0).getView().findViewById(R.id.download);
        assertNotNull(downloadButton);
        downloadButton.performClick();
        verify(mBitmapDownloadRequestJni, never()).downloadBitmap(any(), any(), any());
        assertTrue(qrCodeDialog.getDialog().isShowing());

        // Verify mIsDownloadInProgress was not latched when skipping due to shutdownStarted().
        when(mProfile.shutdownStarted()).thenReturn(false);
        downloadButton.performClick();
        verify(mBitmapDownloadRequestJni)
                .downloadBitmap(eq(mProfile), anyString(), any(Bitmap.class));
    }

    @Test
    public void testRecreatedDialogWithoutProfileDismisses() {
        QrCodeDialog qrCodeDialog = new QrCodeDialog();
        Bundle args = new Bundle();
        args.putString(QrCodeDialog.URL_KEY, TEST_URL);
        qrCodeDialog.setArguments(args);
        qrCodeDialog.setWindowAndroid(mWindowAndroid);
        qrCodeDialog.show(mActivity.getFragmentManager(), null);
        mActivity.getFragmentManager().executePendingTransactions();
        Shadows.shadowOf(Looper.getMainLooper()).idle();

        assertFalse(qrCodeDialog.isAdded());
        assertNull(qrCodeDialog.getDialog());
    }

    @Test
    public void testDialogWithShutdownStartedProfileDismisses() {
        when(mProfile.shutdownStarted()).thenReturn(true);
        QrCodeDialog qrCodeDialog = QrCodeDialog.newInstance(TEST_URL, mWindowAndroid, mProfile);
        qrCodeDialog.show(mActivity.getFragmentManager(), null);
        mActivity.getFragmentManager().executePendingTransactions();
        Shadows.shadowOf(Looper.getMainLooper()).idle();

        assertFalse(qrCodeDialog.isAdded());
        assertNull(qrCodeDialog.getDialog());
    }
}
