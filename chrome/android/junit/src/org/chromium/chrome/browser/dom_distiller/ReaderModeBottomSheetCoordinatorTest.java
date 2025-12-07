// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.dom_distiller;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.any;
import static org.mockito.Mockito.anyBoolean;
import static org.mockito.Mockito.eq;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.UserActionTester;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.theme.ThemeColorProvider;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent.ContentPriority;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.dom_distiller.core.DistilledPagePrefs;
import org.chromium.components.dom_distiller.core.DomDistillerService;

/** Unit tests for {@link ReaderModeBottomSheetCoordinator}. */
@RunWith(BaseRobolectricTestRunner.class)
public class ReaderModeBottomSheetCoordinatorTest {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private BottomSheetController mBottomSheetController;
    @Mock private DomDistillerServiceFactoryJni mDomDistillerServiceFactoryJni;
    @Mock private DomDistillerService mDomDistillerService;
    @Mock private DistilledPagePrefs mDistilledPagePrefs;
    @Mock private Profile mProfile;
    @Mock private Tab mTab;
    @Mock private ThemeColorProvider mThemeColorProvider;
    @Captor private ArgumentCaptor<ThemeColorProvider.ThemeColorObserver> mThemeColorObserverCaptor;
    @Captor private ArgumentCaptor<ThemeColorProvider.TintObserver> mThemeTintObserverCaptor;

    private ReaderModeBottomSheetCoordinator mCoordinator;
    private Activity mActivity;
    private UserActionTester mUserActionTester;

    @Before
    public void setUp() {
        mActivity = Robolectric.buildActivity(Activity.class).create().get();
        mActivity.setTheme(R.style.Theme_BrowserUI_DayNight);

        when(mProfile.getOriginalProfile()).thenReturn(mProfile);
        when(mDomDistillerService.getDistilledPagePrefs()).thenReturn(mDistilledPagePrefs);
        when(mDomDistillerServiceFactoryJni.getForProfile(any())).thenReturn(mDomDistillerService);
        DomDistillerServiceFactoryJni.setInstanceForTesting(mDomDistillerServiceFactoryJni);
        mCoordinator =
                new ReaderModeBottomSheetCoordinator(
                        mActivity, mProfile, mBottomSheetController, mThemeColorProvider);
        mUserActionTester = new UserActionTester();
    }

    @After
    public void tearDown() {
        mUserActionTester.tearDown();
    }

    @Test
    public void testShow() {
        when(mBottomSheetController.requestShowContent(any(), anyBoolean())).thenReturn(true);
        mCoordinator.show(mTab);

        verify(mBottomSheetController).requestShowContent(any(), eq(true));
        verify(mBottomSheetController, times(0)).expandSheet();
        Assert.assertEquals(
                1,
                mUserActionTester.getActionCount("DomDistiller.Android.DistilledPagePrefsOpened"));
    }

    @Test
    public void testBottomSheetContentsAreSwappable() {
        mCoordinator.show(mTab);

        verify(mBottomSheetController).requestShowContent(any(), eq(true));
        BottomSheetContent bottomSheetContent = mCoordinator.getBottomSheetContentForTesting();

        assertEquals(ContentPriority.LOW, bottomSheetContent.getPriority());
        assertTrue(bottomSheetContent.canSuppressInAnyState());
    }

    @Test
    public void testCreateDestroy() {
        // An observer should be added to the theme color provider on creation.
        verify(mThemeColorProvider).addThemeColorObserver(mThemeColorObserverCaptor.capture());
        verify(mThemeColorProvider).addTintObserver(mThemeTintObserverCaptor.capture());

        mCoordinator.destroy();
        verify(mThemeColorProvider).removeThemeColorObserver(mThemeColorObserverCaptor.getValue());
        verify(mThemeColorProvider).removeTintObserver(mThemeTintObserverCaptor.getValue());
    }

    @Test
    public void testTapToExpand() {
        when(mBottomSheetController.requestShowContent(any(), anyBoolean())).thenReturn(true);
        when(mBottomSheetController.getSheetState())
                .thenReturn(BottomSheetController.SheetState.PEEK);
        mCoordinator.show(mTab);

        mCoordinator.getViewForTesting().performClick();
        verify(mBottomSheetController).expandSheet();
    }
}
