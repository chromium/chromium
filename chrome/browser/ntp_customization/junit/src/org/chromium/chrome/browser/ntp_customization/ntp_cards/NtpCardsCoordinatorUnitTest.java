// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp_customization.ntp_cards;

import static org.junit.Assert.assertNotNull;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.verify;

import android.content.Context;
import android.view.ContextThemeWrapper;

import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.ContextUtils;
import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.magic_stack.HomeModulesConfigManager;
import org.chromium.chrome.browser.magic_stack.HomeModulesConfigManager.HomeModulesStateListener;
import org.chromium.chrome.browser.magic_stack.ModuleRegistry;
import org.chromium.chrome.browser.ntp_customization.BottomSheetDelegate;
import org.chromium.chrome.browser.ntp_customization.R;
import org.chromium.chrome.browser.profiles.Profile;

/** Unit tests for {@link NtpCardsCoordinator} */
@RunWith(BaseRobolectricTestRunner.class)
@EnableFeatures(ChromeFeatureList.HOME_MODULE_PREF_REFACTOR)
public class NtpCardsCoordinatorUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private BottomSheetDelegate mBottomSheetDelegate;
    @Mock private Profile mProfile;
    @Mock private NtpCardsMediator mMediator;
    @Mock private HomeModulesConfigManager mHomeModulesConfigManager;
    @Mock private ModuleRegistry mModuleRegistry;
    @Captor private ArgumentCaptor<HomeModulesStateListener> mListener;

    private NtpCardsCoordinator mCoordinator;
    private Context mContext;

    @Before
    public void setUp() {
        mContext =
                new ContextThemeWrapper(
                        ContextUtils.getApplicationContext(), R.style.Theme_BrowserUI_DayNight);
        HomeModulesConfigManager.setInstanceForTesting(mHomeModulesConfigManager);
        mCoordinator =
                new NtpCardsCoordinator(
                        mContext,
                        mBottomSheetDelegate,
                        ObservableSuppliers.createNonNull(mProfile),
                        mModuleRegistry);
    }

    @Test
    @SmallTest
    public void testAddsAndRemovesObserver() {
        verify(mHomeModulesConfigManager).addListener(mListener.capture());

        mCoordinator.destroy();
        verify(mHomeModulesConfigManager).removeListener(mListener.getValue());
    }

    @Test
    @SmallTest
    public void testObserverRespondsToSignal() {
        verify(mHomeModulesConfigManager).addListener(mListener.capture());

        mCoordinator.setMediatorForTesting(mMediator);

        mListener.getValue().allCardsConfigChanged(true);
        verify(mMediator).onAllCardsConfigChanged(true);

        mListener.getValue().allCardsConfigChanged(false);
        verify(mMediator).onAllCardsConfigChanged(false);
    }

    @Test
    @SmallTest
    public void testConstructor() {
        assertNotNull(mCoordinator.getMediatorForTesting());
    }

    @Test
    @SmallTest
    public void testDestroy() {
        NtpCardsMediator mediator = mock(NtpCardsMediator.class);
        mCoordinator.setMediatorForTesting(mediator);

        mCoordinator.destroy();
        verify(mediator).destroy();
    }

    @Test
    @SmallTest
    public void testOnAllCardsConfigChanged() {
        NtpCardsMediator mediator = mock(NtpCardsMediator.class);
        mCoordinator.setMediatorForTesting(mediator);

        mCoordinator.onAllCardsConfigChanged(true);
        verify(mediator).onAllCardsConfigChanged(true);

        mCoordinator.onAllCardsConfigChanged(false);
        verify(mediator).onAllCardsConfigChanged(false);
    }
}
