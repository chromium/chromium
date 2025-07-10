// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp_customization.theme;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import static org.chromium.chrome.browser.ntp_customization.NtpCustomizationViewProperties.BACK_PRESS_HANDLER;
import static org.chromium.chrome.browser.ntp_customization.theme.NtpThemeCoordinator.NTPThemeBottomSheetSection.CHROME_COLORS;
import static org.chromium.chrome.browser.ntp_customization.theme.NtpThemeCoordinator.NTPThemeBottomSheetSection.THEME_COLLECTIONS;
import static org.chromium.chrome.browser.ntp_customization.theme.NtpThemeProperty.IS_SECTION_TRAILING_ICON_VISIBLE;
import static org.chromium.chrome.browser.ntp_customization.theme.NtpThemeProperty.LEADING_ICON_FOR_THEME_COLLECTIONS;
import static org.chromium.chrome.browser.ntp_customization.theme.NtpThemeProperty.LEARN_MORE_BUTTON_CLICK_LISTENER;
import static org.chromium.chrome.browser.ntp_customization.theme.NtpThemeProperty.SECTION_ON_CLICK_LISTENER;

import android.app.Activity;
import android.util.Pair;
import android.view.View;
import android.view.View.OnClickListener;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.ntp_customization.BottomSheetDelegate;
import org.chromium.chrome.browser.ntp_customization.NtpCustomizationConfigManager;
import org.chromium.chrome.browser.ntp_customization.NtpCustomizationCoordinator.BottomSheetType;
import org.chromium.chrome.browser.ntp_customization.NtpCustomizationViewProperties;
import org.chromium.chrome.browser.ntp_customization.theme.theme_collections.NtpThemeCollectionsCoordinator;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.ui.modelutil.PropertyModel;

/** Tests for {@link NtpThemeMediator}. */
@RunWith(BaseRobolectricTestRunner.class)
public class NtpThemeMediatorUnitTest {

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private BottomSheetDelegate mBottomSheetDelegate;
    @Mock private Profile mProfile;
    @Mock private View mView;
    @Mock private NtpCustomizationConfigManager mNtpCustomizationConfigManager;
    @Mock private NtpThemeCollectionsCoordinator mNtpThemeCollectionsCoordinator;

    private PropertyModel mBottomSheetPropertyModel;
    private PropertyModel mThemePropertyModel;
    private NtpThemeMediator mMediator;
    private Activity mActivity;

    @Before
    public void setUp() {
        mActivity = Robolectric.buildActivity(Activity.class).setup().get();
        when(mView.getContext()).thenReturn(mActivity);

        mBottomSheetPropertyModel =
                new PropertyModel(NtpCustomizationViewProperties.BOTTOM_SHEET_KEYS);
        mThemePropertyModel = new PropertyModel(NtpThemeProperty.THEME_KEYS);
    }

    @Test
    public void testConstructor_shouldShowAlone() {
        createMediator(/* shouldShowAlone= */ true);

        assertNull(mBottomSheetPropertyModel.get(BACK_PRESS_HANDLER));
        assertNotNull(mThemePropertyModel.get(LEARN_MORE_BUTTON_CLICK_LISTENER));
        assertNotNull(mThemePropertyModel.get(SECTION_ON_CLICK_LISTENER));
    }

    @Test
    public void testConstructor_shouldNotShowAlone() {
        createMediator(/* shouldShowAlone= */ false);

        assertNotNull(mBottomSheetPropertyModel.get(BACK_PRESS_HANDLER));
        mBottomSheetPropertyModel.get(BACK_PRESS_HANDLER).onClick(mView);
        verify(mBottomSheetDelegate).backPressOnCurrentBottomSheet();
    }

    @Test
    public void testDestroy() {
        createMediator(/* shouldShowAlone= */ false);
        mMediator.setNtpThemeCollectionsCoordinatorForTesting(mNtpThemeCollectionsCoordinator);

        assertNotNull(mBottomSheetPropertyModel.get(BACK_PRESS_HANDLER));
        assertNotNull(mThemePropertyModel.get(LEARN_MORE_BUTTON_CLICK_LISTENER));

        mMediator.destroy();

        assertNull(mBottomSheetPropertyModel.get(BACK_PRESS_HANDLER));
        assertNull(mThemePropertyModel.get(LEARN_MORE_BUTTON_CLICK_LISTENER));

        verify(mNtpThemeCollectionsCoordinator).destroy();
    }

    @Test
    public void testOnClickListeners() {
        createMediator(/* shouldShowAlone= */ true);

        Pair<Integer, OnClickListener> listenerPair =
                mThemePropertyModel.get(SECTION_ON_CLICK_LISTENER);
        assertEquals(THEME_COLLECTIONS, (int) listenerPair.first);
        assertNotNull(listenerPair.second);
        listenerPair.second.onClick(mView);

        Pair<Integer, Boolean> visibilityPair =
                mThemePropertyModel.get(IS_SECTION_TRAILING_ICON_VISIBLE);
        assertEquals(CHROME_COLORS, (int) visibilityPair.first);
        assertEquals(false, visibilityPair.second);

        mMediator.handleChromeColorsSectionClick(mView);
        visibilityPair = mThemePropertyModel.get(IS_SECTION_TRAILING_ICON_VISIBLE);
        assertEquals(CHROME_COLORS, (int) visibilityPair.first);
        assertEquals(true, visibilityPair.second);
    }

    @Test
    public void testOnClickListeners_DefaultSectionClick() {
        createMediator(/* shouldShowAlone= */ true);

        mMediator.handleChromeDefaultSectionClick(mView);
        verify(mNtpCustomizationConfigManager).onBackgroundChanged(eq(null));
    }

    @Test
    public void testHandleThemeCollectionsSectionClick() {
        createMediator(/* shouldShowAlone= */ true);

        mMediator.handleThemeCollectionsSectionClick(mView);

        // Verify it tries to show the theme collections bottom sheet.
        verify(mBottomSheetDelegate).showBottomSheet(eq(BottomSheetType.THEME_COLLECTIONS));
    }

    @Test
    public void testSetLeadingIconForThemeCollectionsSection() {
        createMediator(/* shouldShowAlone= */ true);

        Pair<Integer, Integer> drawablePair =
                mThemePropertyModel.get(LEADING_ICON_FOR_THEME_COLLECTIONS);
        assertNotNull(drawablePair);
    }

    private void createMediator(boolean shouldShowAlone) {
        when(mBottomSheetDelegate.shouldShowAlone()).thenReturn(shouldShowAlone);
        mMediator =
                new NtpThemeMediator(
                        mActivity,
                        mBottomSheetPropertyModel,
                        mThemePropertyModel,
                        mBottomSheetDelegate,
                        mProfile,
                        mNtpCustomizationConfigManager,
                        null);
    }
}
