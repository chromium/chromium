// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments.handler;

import static org.junit.Assert.assertNull;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.content.WebContentsFactory;
import org.chromium.chrome.browser.payments.handler.PaymentHandlerCoordinator.PaymentHandlerUiObserver;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorSupplier;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetControllerProvider;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.url.GURL;
import org.chromium.url.JUnitTestOrigins;

import java.lang.ref.WeakReference;

/** Unit tests for {@link PaymentHandlerCoordinator}. */
@RunWith(BaseRobolectricTestRunner.class)
public class PaymentHandlerCoordinatorUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private WebContents mPaymentRequestWebContents;
    @Mock private WindowAndroid mWindowAndroid;
    @Mock private Profile mProfile;
    @Mock private BottomSheetController mBottomSheetController;
    @Mock private TabModelSelector mTabModelSelector;
    @Mock private Tab mTab;
    @Mock private WebContents mPaymentHandlerWebContents;
    @Mock private PaymentHandlerUiObserver mUiObserver;

    private Activity mActivity;
    private PaymentHandlerCoordinator mCoordinator;

    @Before
    public void setUp() {
        mActivity = Robolectric.buildActivity(Activity.class).setup().get();
        when(mWindowAndroid.getActivity()).thenReturn(new WeakReference<>(mActivity));
        when(mPaymentRequestWebContents.getTopLevelNativeWindow()).thenReturn(mWindowAndroid);
        Profile.setProfileFromWebContentsForTesting(mProfile);
        BottomSheetControllerProvider.setInstanceForTesting(mBottomSheetController);
        TabModelSelectorSupplier.setInstanceForTesting(mTabModelSelector);
        when(mTabModelSelector.getCurrentTab()).thenReturn(mTab);
        WebContentsFactory.setWebContentsForTesting(mPaymentHandlerWebContents);

        mCoordinator = new PaymentHandlerCoordinator();
    }

    @Test
    public void testShow_profileShutdownStarted_returnsNullAndCreatesNoWebContents() {
        when(mProfile.shutdownStarted()).thenReturn(true);

        WebContents result =
                mCoordinator.show(
                        mPaymentRequestWebContents,
                        new GURL("https://example.com"),
                        JUnitTestOrigins.createTuple("https", "example.com", 443),
                        mUiObserver);

        assertNull(result);
        assertNull(mCoordinator.getWebContentsForTest());
        verify(mPaymentHandlerWebContents, never()).getNavigationController();

        ProfileManager.onProfileDestroyed(mProfile);
        verify(mUiObserver, never()).onPaymentHandlerUiClosed();
    }
}
