// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.dom_distiller;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.any;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.content.Context;
import android.view.ContextThemeWrapper;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.ContextUtils;
import org.chromium.base.FeatureOverrides;
import org.chromium.base.UnownedUserDataHost;
import org.chromium.base.UserDataHost;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.task.TaskTraits;
import org.chromium.base.task.test.ShadowPostTask;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.dom_distiller.ReaderModeManager.EntryPoint;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.toolbar.optional_button.ButtonData;
import org.chromium.chrome.browser.toolbar.optional_button.ButtonDataProvider;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetControllerFactory;
import org.chromium.components.browser_ui.bottomsheet.ManagedBottomSheetController;
import org.chromium.components.dom_distiller.core.DistilledPagePrefs;
import org.chromium.components.dom_distiller.core.DomDistillerFeatures;
import org.chromium.components.dom_distiller.core.DomDistillerService;
import org.chromium.components.dom_distiller.core.DomDistillerUrlUtilsJni;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.url.GURL;

/** This class tests the behavior of the {@link ReaderModeToolbarButtonController}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(shadows = {ShadowPostTask.class})
public class ReaderModeToolbarButtonControllerTest {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private Tab mMockTab;
    @Mock private WindowAndroid mWindowAndroid;
    @Mock private ReaderModeManager mMockReaderModeManager;
    @Mock private ActivityTabProvider mMockActivityTabProvider;
    @Mock private ModalDialogManager mMockModalDialogManager;
    @Mock private DomDistillerUrlUtilsJni mDomDistillerUrlUtilsJni;
    @Mock private Profile mProfile;
    @Mock private DomDistillerService mDomDistillerService;
    @Mock private DomDistillerServiceFactoryJni mDomDistillerServiceFactoryJni;
    @Mock private DistilledPagePrefs mDistilledPagePrefs;
    @Mock private ManagedBottomSheetController mBottomSheetController;
    @Mock private ReaderModeActionRateLimiter mReaderModeActionRateLimiter;
    @Mock private ReaderModeIphController mReaderModeIphController;

    private final ObservableSupplierImpl<Profile> mProfileSupplier = new ObservableSupplierImpl<>();
    private final ObservableSupplierImpl<ReaderModeIphController> mReaderModeIphControllerSupplier =
            new ObservableSupplierImpl<>();

    private UserDataHost mUserDataHost;
    private UnownedUserDataHost mUnownedUserDataHost;
    private Context mContext;

    @Before
    public void setUp() throws Exception {
        ShadowPostTask.setTestImpl(
                new ShadowPostTask.TestImpl() {
                    @Override
                    public void postDelayedTask(
                            @TaskTraits int taskTraits, Runnable task, long delay) {
                        task.run();
                    }
                });

        mUserDataHost = new UserDataHost();
        mUnownedUserDataHost = new UnownedUserDataHost();

        mContext =
                new ContextThemeWrapper(
                        ContextUtils.getApplicationContext(), R.style.Theme_BrowserUI_DayNight);

        ReaderModeActionRateLimiter.setInstanceForTesting(mReaderModeActionRateLimiter);
        when(mWindowAndroid.getUnownedUserDataHost()).thenReturn(mUnownedUserDataHost);
        BottomSheetControllerFactory.attach(mWindowAndroid, mBottomSheetController);
        when(mMockTab.getWindowAndroid()).thenReturn(mWindowAndroid);
        when(mMockTab.getProfile()).thenReturn(mProfile);
        when(mProfile.getOriginalProfile()).thenReturn(mProfile);
        mProfileSupplier.set(mProfile);
        when(mMockTab.getContext()).thenReturn(mContext);
        when(mMockActivityTabProvider.get()).thenReturn(mMockTab);
        when(mMockTab.getUserDataHost()).thenReturn(mUserDataHost);
        mUserDataHost.setUserData(ReaderModeManager.USER_DATA_KEY, mMockReaderModeManager);
        mReaderModeIphControllerSupplier.set(mReaderModeIphController);

        when(mDomDistillerService.getDistilledPagePrefs()).thenReturn(mDistilledPagePrefs);
        when(mDomDistillerServiceFactoryJni.getForProfile(any())).thenReturn(mDomDistillerService);
        DomDistillerServiceFactoryJni.setInstanceForTesting(mDomDistillerServiceFactoryJni);
        DomDistillerUrlUtilsJni.setInstanceForTesting(mDomDistillerUrlUtilsJni);

        FeatureOverrides.enable(ChromeFeatureList.ADAPTIVE_BUTTON_IN_TOP_TOOLBAR_CUSTOMIZATION_V2);
    }

    private ReaderModeToolbarButtonController createController() {
        return new ReaderModeToolbarButtonController(
                mContext,
                mProfileSupplier,
                mMockActivityTabProvider,
                mMockModalDialogManager,
                mReaderModeIphControllerSupplier);
    }

    @Test
    public void testButtonClickNonDistilledPage_EnablesReaderMode() {
        ReaderModeToolbarButtonController controller = createController();

        ButtonData readerModeButton = controller.get(mMockTab);
        readerModeButton.getButtonSpec().getOnClickListener().onClick(null);
        verify(mReaderModeActionRateLimiter).onActionClicked();
        verify(mMockReaderModeManager).activateReaderMode(EntryPoint.TOOLBAR_BUTTON);
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
        assertEquals(R.string.show_reading_mode_text, controller.getButtonDataForTesting().getButtonSpec().getHoverTooltipTextId());

        // Simulate the url changing to reader mode, and verify that the button was swapped.
        when(mMockTab.getUrl()).thenReturn(new GURL("chrome-distiller://test"));
        when(mDomDistillerUrlUtilsJni.isDistilledPage(any())).thenReturn(true);
        controller.getTabSupplierObserverForTesting().onUrlUpdated(mMockTab);
        assertEquals("Hide Reading mode",
                controller.getButtonDataForTesting().getButtonSpec().getContentDescription());
        assertEquals(R.string.hide_reading_mode_text, controller.getButtonDataForTesting().getButtonSpec().getHoverTooltipTextId());
        assertTrue(controller.getButtonDataForTesting().getButtonSpec().isChecked());

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
        assertEquals("Hide Reading mode",
                controller.getButtonDataForTesting().getButtonSpec().getContentDescription());

        // Now do the same thing with a null tab.
        when(mMockActivityTabProvider.get()).thenReturn(null);
        controller.getTabSupplierObserverForTesting().onUrlUpdated(null);
        assertEquals(
                R.string.reader_mode_cpa_button_text,
                controller.getButtonDataForTesting().getButtonSpec().getActionChipLabelResId());
    }

    @Test
    @EnableFeatures(DomDistillerFeatures.READER_MODE_DISTILL_IN_APP + ":hide_cpa_delay_ms/0")
    public void testReaderModeButton_timesOut() throws Exception {
        ReaderModeToolbarButtonController controller = createController();


        when(mMockTab.getUrl()).thenReturn(new GURL("chrome-distiller://test"));
        when(mDomDistillerUrlUtilsJni.isDistilledPage(any())).thenReturn(true);
        controller.getTabSupplierObserverForTesting().onUrlUpdated(mMockTab);
        assertTrue(controller.shouldShowButton(mMockTab));

        CallbackHelper callbackHelper = new CallbackHelper();
        ButtonDataProvider.ButtonDataObserver observer =
                new ButtonDataProvider.ButtonDataObserver() {
                    @Override
                    public void buttonDataChanged(boolean canShowHint) {
                        Assert.assertFalse(canShowHint);
                        callbackHelper.notifyCalled();
                        controller.removeObserver(this);
                    }
                };
        controller.addObserver(observer);

        HistogramWatcher watcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                ReaderModeMetrics
                                        .READER_MODE_CONTEXTUAL_PAGE_ACTION_EVENT_HISTOGRAM,
                                ReaderModeMetrics.ReaderModeContextualPageActionEvent.TIME_OUT)
                        .build();

        // Simulate the button being shown, and verify that the button is hidden after a delay.
        controller.onActionShown();
        callbackHelper.waitForNext();
        assertFalse(controller.shouldShowButton(mMockTab));

        watcher.assertExpected();
    }

    @Test
    @DisableFeatures(DomDistillerFeatures.READER_MODE_DISTILL_IN_APP)
    public void testReaderModeShouldShowButton_whenDistillInAppDisabled() throws Exception {
        // When ReaderModeDistillInApp is disabled, the button should always be "available" to be
        // shown. The actual showing of the button is driven through ReaderModeActionProvider.
        ReaderModeToolbarButtonController controller = createController();

        when(mMockTab.getUrl()).thenReturn(new GURL("chrome-distiller://test"));
        when(mDomDistillerUrlUtilsJni.isDistilledPage(any())).thenReturn(true);
        controller.getTabSupplierObserverForTesting().onUrlUpdated(mMockTab);
        assertTrue(controller.shouldShowButton(mMockTab));

        when(mMockTab.getUrl()).thenReturn(new GURL("http://test.com"));
        when(mDomDistillerUrlUtilsJni.isDistilledPage(any())).thenReturn(false);
        controller.getTabSupplierObserverForTesting().onUrlUpdated(mMockTab);
        assertTrue(controller.shouldShowButton(mMockTab));
    }
}
