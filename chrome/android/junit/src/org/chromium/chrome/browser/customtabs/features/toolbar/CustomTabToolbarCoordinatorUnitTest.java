// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs.features.toolbar;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.isNull;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import static org.chromium.chrome.browser.flags.ChromeFeatureList.SHARE_CUSTOM_ACTIONS_IN_CCT;

import android.app.Activity;
import android.app.PendingIntent;
import android.content.Intent;

import dagger.Lazy;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.Robolectric;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.browser_controls.BrowserControlsVisibilityManager;
import org.chromium.chrome.browser.browserservices.intents.CustomButtonParams;
import org.chromium.chrome.browser.customtabs.CloseButtonVisibilityManager;
import org.chromium.chrome.browser.customtabs.CustomButtonParamsImpl;
import org.chromium.chrome.browser.customtabs.CustomTabCompositorContentInitializer;
import org.chromium.chrome.browser.customtabs.content.CustomTabActivityContentTestEnvironment;
import org.chromium.chrome.browser.customtabs.content.CustomTabActivityTabController;
import org.chromium.chrome.browser.customtabs.content.CustomTabActivityTabProvider;
import org.chromium.chrome.browser.share.ShareDelegate;
import org.chromium.chrome.browser.share.ShareDelegateSupplier;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.ui.base.ActivityWindowAndroid;
import org.chromium.url.GURL;

/** Tests for {@link CustomTabToolbarCoordinator}. */
@RunWith(BaseRobolectricTestRunner.class)
@EnableFeatures({SHARE_CUSTOM_ACTIONS_IN_CCT})
public class CustomTabToolbarCoordinatorUnitTest {
    @Rule
    public final CustomTabActivityContentTestEnvironment env =
            new CustomTabActivityContentTestEnvironment();

    @Mock private ShareDelegate mShareDelegate;
    @Mock private ShareDelegateSupplier mShareDelegateSupplier;
    @Mock private CustomTabActivityTabProvider mTabProvider;
    @Mock private ActivityWindowAndroid mActivityWindowAndroid;
    @Mock private Lazy<BrowserControlsVisibilityManager> mBrowserControlsVisibilityManager;
    @Mock private CloseButtonVisibilityManager mCloseButtonVisibilityManager;
    @Mock private CustomTabBrowserControlsVisibilityDelegate mVisibilityDelegate;
    @Mock private CustomTabCompositorContentInitializer mCompositorContentInitializer;
    @Mock private CustomTabToolbarColorController mToolbarColorController;
    @Mock private Tab mTab;
    @Mock private CustomButtonParams mCustomButtonParams;
    @Mock private PendingIntent mPendingIntent;

    private Activity mActivity;
    private CustomTabActivityTabController mTabController;
    private CustomTabToolbarCoordinator mCoordinator;

    @Before
    public void setup() {
        MockitoAnnotations.initMocks(this);

        mActivity = Robolectric.setupActivity(Activity.class);
        mTabController = env.createTabController();

        mCoordinator =
                new CustomTabToolbarCoordinator(
                        env.intentDataProvider,
                        mTabProvider,
                        env.connection,
                        mActivity,
                        mActivityWindowAndroid,
                        mActivity,
                        mTabController,
                        mBrowserControlsVisibilityManager,
                        env.createNavigationController(mTabController),
                        mCloseButtonVisibilityManager,
                        mVisibilityDelegate,
                        mCompositorContentInitializer,
                        mToolbarColorController);

        ShareDelegateSupplier.setInstanceForTesting(mShareDelegateSupplier);
        when(mShareDelegateSupplier.get()).thenReturn(mShareDelegate);
        when(mTabProvider.getTab()).thenReturn(mTab);
        when(mTab.getOriginalUrl()).thenReturn(GURL.emptyGURL());
        when(mTab.getTitle()).thenReturn("");
        when(mCustomButtonParams.getDescription()).thenReturn("");
        when(mCustomButtonParams.getPendingIntent()).thenReturn(mPendingIntent);
    }

    @After
    public void tearDown() {
        mActivity.finish();
    }

    private void clickButtonAndVerifyPendingIntent() {
        try {
            mCoordinator.onCustomButtonClick(mCustomButtonParams);
            verify(mShareDelegate, never()).share(any(Tab.class), eq(false), anyInt());
            verify(mPendingIntent)
                    .send(
                            eq(mActivity),
                            eq(0),
                            any(Intent.class),
                            any(),
                            isNull(),
                            isNull(),
                            any());
        } catch (PendingIntent.CanceledException e) {
            assert false;
        }
    }

    @Test
    public void testCreateShareButtonWithCustomActions() {
        int testColor = 0x99aabbcc;
        mCoordinator.onCustomButtonClick(
                CustomButtonParamsImpl.createShareButton(mActivity, testColor));
        verify(mShareDelegate)
                .share(any(), eq(false), eq(ShareDelegate.ShareOrigin.CUSTOM_TAB_SHARE_BUTTON));
    }

    @Test
    public void testCustomButtonClicked() {
        when(mCustomButtonParams.getType()).thenReturn(CustomButtonParams.ButtonType.OTHER);
        clickButtonAndVerifyPendingIntent();
    }

    @Test
    public void testNullSupplierShareButtonClick() {
        when(mCustomButtonParams.getType())
                .thenReturn(CustomButtonParams.ButtonType.CCT_SHARE_BUTTON);

        // Test null supplier.
        when(mShareDelegateSupplier.get()).thenReturn(null);
        clickButtonAndVerifyPendingIntent();
    }

    @Test
    @DisableFeatures({SHARE_CUSTOM_ACTIONS_IN_CCT})
    public void testShareWithoutCustomActions() {
        when(mCustomButtonParams.getType())
                .thenReturn(CustomButtonParams.ButtonType.CCT_SHARE_BUTTON);

        clickButtonAndVerifyPendingIntent();
    }
}
