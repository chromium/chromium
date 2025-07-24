// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.dom_distiller;

import static org.junit.Assert.assertEquals;
import static org.mockito.Mockito.any;
import static org.mockito.Mockito.eq;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.content.Context;
import android.view.ContextThemeWrapper;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.ContextUtils;
import org.chromium.base.FeatureOverrides;
import org.chromium.base.UserDataHost;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.toolbar.optional_button.ButtonData;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.dom_distiller.core.DistilledPagePrefs;
import org.chromium.components.dom_distiller.core.DomDistillerFeatures;
import org.chromium.components.dom_distiller.core.DomDistillerService;
import org.chromium.components.dom_distiller.core.DomDistillerUrlUtilsJni;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.url.GURL;

/** This class tests the behavior of the {@link ReaderModeToolbarButtonController}. */
@RunWith(BaseRobolectricTestRunner.class)
public class ReaderModeToolbarButtonControllerTest {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Mock private Tab mMockTab;
    @Mock private ReaderModeManager mMockReaderModeManager;
    @Mock private ActivityTabProvider mMockActivityTabProvider;
    @Mock private ModalDialogManager mMockModalDialogManager;
    @Mock private DomDistillerUrlUtilsJni mDomDistillerUrlUtilsJni;
    @Mock private Profile mProfile;
    @Mock private BottomSheetController mBottomSheetController;
    @Mock private DomDistillerService mDomDistillerService;
    @Mock private DomDistillerServiceFactoryJni mDomDistillerServiceFactoryJni;
    @Mock private DistilledPagePrefs mDistilledPagePrefs;

    private final ObservableSupplierImpl<Profile> mProfileSupplier = new ObservableSupplierImpl<>();
    private UserDataHost mUserDataHost;

    @Before
    public void setUp() throws Exception {
        mUserDataHost = new UserDataHost();

        Context context =
                new ContextThemeWrapper(
                        ContextUtils.getApplicationContext(), R.style.Theme_BrowserUI_DayNight);

        when(mProfile.getOriginalProfile()).thenReturn(mProfile);
        mProfileSupplier.set(mProfile);
        when(mMockTab.getContext()).thenReturn(context);
        when(mMockActivityTabProvider.get()).thenReturn(mMockTab);
        when(mMockTab.getUserDataHost()).thenReturn(mUserDataHost);
        mUserDataHost.setUserData(ReaderModeManager.USER_DATA_KEY, mMockReaderModeManager);

        when(mDomDistillerService.getDistilledPagePrefs()).thenReturn(mDistilledPagePrefs);
        when(mDomDistillerServiceFactoryJni.getForProfile(any())).thenReturn(mDomDistillerService);
        DomDistillerServiceFactoryJni.setInstanceForTesting(mDomDistillerServiceFactoryJni);
        DomDistillerUrlUtilsJni.setInstanceForTesting(mDomDistillerUrlUtilsJni);

        FeatureOverrides.enable(ChromeFeatureList.ADAPTIVE_BUTTON_IN_TOP_TOOLBAR_CUSTOMIZATION_V2);
    }

    private ReaderModeToolbarButtonController createController() {
        return new ReaderModeToolbarButtonController(
                mMockTab.getContext(),
                mProfileSupplier,
                mMockActivityTabProvider,
                mMockModalDialogManager,
                mBottomSheetController);
    }

    @Test
    public void testButtonClickNonDistilledPage_EnablesReaderMode() {
        ReaderModeToolbarButtonController controller = createController();

        ButtonData readerModeButton = controller.get(mMockTab);
        readerModeButton.getButtonSpec().getOnClickListener().onClick(null);

        verify(mMockReaderModeManager).activateReaderMode();
    }

    @Test
    @EnableFeatures(DomDistillerFeatures.READER_MODE_DISTILL_IN_APP)
    public void testButtonClickDistilledPage_HidesReaderMode() {
        ReaderModeToolbarButtonController controller = createController();

        when(mMockTab.getUrl()).thenReturn(new GURL("chrome-distiller://test"));
        when(mDomDistillerUrlUtilsJni.isDistilledPage(any())).thenReturn(true);

        ButtonData readerModeButton = controller.get(mMockTab);
        readerModeButton.getButtonSpec().getOnClickListener().onClick(null);

        verify(mMockReaderModeManager).hideReaderMode();
    }

    @Test
    @EnableFeatures(DomDistillerFeatures.READER_MODE_DISTILL_IN_APP)
    public void buttonSpecChangesWhenInReaderMode() {
        ReaderModeToolbarButtonController controller = createController();
        assertEquals(
                R.string.reader_mode_cpa_button_text,
                controller.getButtonDataForTesting().getButtonSpec().getActionChipLabelResId());

        // Simulate the url changing to reader mode, and verify that the button was swapped.
        when(mMockTab.getUrl()).thenReturn(new GURL("chrome-distiller://test"));
        when(mDomDistillerUrlUtilsJni.isDistilledPage(any())).thenReturn(true);
        controller.getTabSupplierObserverForTesting().onUrlUpdated(mMockTab);
        assertEquals(
                R.string.hide_reading_mode_text,
                controller.getButtonDataForTesting().getButtonSpec().getActionChipLabelResId());

        // Simulate the url changing to something else, and verify that the button was swapped back.
        when(mMockTab.getUrl()).thenReturn(new GURL("http://test.com"));
        when(mDomDistillerUrlUtilsJni.isDistilledPage(any())).thenReturn(false);
        controller.getTabSupplierObserverForTesting().onUrlUpdated(mMockTab);
        assertEquals(
                R.string.reader_mode_cpa_button_text,
                controller.getButtonDataForTesting().getButtonSpec().getActionChipLabelResId());

        // Flip it back
        when(mMockTab.getUrl()).thenReturn(new GURL("chrome-distiller://test"));
        when(mDomDistillerUrlUtilsJni.isDistilledPage(any())).thenReturn(true);
        controller.getTabSupplierObserverForTesting().onUrlUpdated(mMockTab);
        assertEquals(
                R.string.hide_reading_mode_text,
                controller.getButtonDataForTesting().getButtonSpec().getActionChipLabelResId());

        // Now do the same thing with a null tab.
        when(mMockActivityTabProvider.get()).thenReturn(null);
        controller.getTabSupplierObserverForTesting().onUrlUpdated(null);
        assertEquals(
                R.string.reader_mode_cpa_button_text,
                controller.getButtonDataForTesting().getButtonSpec().getActionChipLabelResId());
    }

    @Test
    @EnableFeatures(DomDistillerFeatures.READER_MODE_DISTILL_IN_APP)
    public void testBottomSheetShownOnUrlChange() {
        ReaderModeToolbarButtonController controller = createController();

        // Verify URL changes for non-distilled pages do nothing.
        when(mMockTab.getUrl()).thenReturn(new GURL("http://test.com"));
        when(mDomDistillerUrlUtilsJni.isDistilledPage(any())).thenReturn(false);
        controller.getTabSupplierObserverForTesting().onUrlUpdated(mMockTab);
        verify(mBottomSheetController, times(0)).requestShowContent(any(), eq(true));

        // Verify URL changes for distilled pages show the bottom sheet.
        when(mMockTab.getUrl()).thenReturn(new GURL("chrome-distiller://test"));
        when(mDomDistillerUrlUtilsJni.isDistilledPage(any())).thenReturn(true);
        controller.getTabSupplierObserverForTesting().onUrlUpdated(mMockTab);
        verify(mBottomSheetController).requestShowContent(any(), eq(true));

        // Verify URL updates for the same URL don't trigger again.
        controller.getTabSupplierObserverForTesting().onUrlUpdated(mMockTab);
        verify(mBottomSheetController, times(1)).requestShowContent(any(), eq(true));
    }
}
