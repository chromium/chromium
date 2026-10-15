// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.when;

import android.content.Context;

import androidx.test.core.app.ApplicationProvider;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.concurrent.atomic.AtomicReference;

/**
 * Tests fo the close all tabs dialog to confirm the close all tabs action.
 *
 * <p>This test assumes that the Modal Dialog UI component is well tested responds correctly to user
 * inputs. It only tests the logic and properties of the dialog.
 */
@RunWith(BaseRobolectricTestRunner.class)
public class CloseAllTabsDialogUnitTest {
    private static class MockModalDialogManager extends ModalDialogManager {
        private PropertyModel mDialogModel;
        private @ModalDialogManager.ModalDialogType int mDialogType;

        public MockModalDialogManager() {
            super(Mockito.mock(ModalDialogManager.Presenter.class), 0);
        }

        @Override
        public void showDialog(
                PropertyModel model,
                @ModalDialogManager.ModalDialogType int dialogType,
                boolean showNext) {
            mDialogModel = model;
            mDialogType = dialogType;
        }

        public PropertyModel getDialogModel() {
            return mDialogModel;
        }

        public @ModalDialogManager.ModalDialogType int getDialogType() {
            return mDialogType;
        }

        public void simulateButtonClick(@ModalDialogProperties.ButtonType int buttonType) {
            mDialogModel.get(ModalDialogProperties.CONTROLLER).onClick(mDialogModel, buttonType);
        }

        public void simulateCheckboxToggle(boolean isChecked) {
            mDialogModel.set(ModalDialogProperties.CHECKBOX_CHECKED, isChecked);
        }

        @Override
        public void dismissDialog(PropertyModel model, @DialogDismissalCause int dismissalCause) {
            assertEquals(model, mDialogModel);
            mDialogModel
                    .get(ModalDialogProperties.CONTROLLER)
                    .onDismiss(mDialogModel, dismissalCause);
            mDialogModel = null;
            mDialogType = -1;
        }
    }

    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    private Context mContext;
    private MockModalDialogManager mMockModalDialogManager;
    private boolean mRunnableCalled;

    @Mock private TabModelSelector mTabModelSelectorMock;
    @Mock private TabModel mTabModelMock;
    @Mock private TabModel mIncognitoTabModelMock;

    @Before
    public void setUp() {
        when(mTabModelSelectorMock.getModel(false)).thenReturn(mTabModelMock);
        when(mTabModelSelectorMock.getModel(true)).thenReturn(mIncognitoTabModelMock);
        mContext = ApplicationProvider.getApplicationContext();
        mMockModalDialogManager = new MockModalDialogManager();
        mRunnableCalled = false;
    }

    private ModalDialogManager getModalDialogManager() {
        return mMockModalDialogManager;
    }

    private void verifyModel(boolean isIncognito) {
        assertEquals(
                ModalDialogManager.ModalDialogType.APP, mMockModalDialogManager.getDialogType());

        final PropertyModel model = mMockModalDialogManager.getDialogModel();
        assertNotNull(model);
        assertEquals(
                mContext.getString(
                        isIncognito
                                ? R.string.close_all_tabs_dialog_title_incognito
                                : R.string.close_all_tabs_dialog_title),
                model.get(ModalDialogProperties.TITLE));
        assertEquals(
                CloseAllTabsDialog.getDialogDescriptionString(
                        mContext, mTabModelSelectorMock, isIncognito),
                model.get(ModalDialogProperties.MESSAGE_PARAGRAPH_1));
        assertEquals(
                mContext.getString(R.string.close_all_tabs_and_groups_action),
                model.get(ModalDialogProperties.POSITIVE_BUTTON_TEXT));
        assertEquals(
                mContext.getString(R.string.cancel),
                model.get(ModalDialogProperties.NEGATIVE_BUTTON_TEXT));
        assertTrue(model.get(ModalDialogProperties.CANCEL_ON_TOUCH_OUTSIDE));
        assertEquals(
                ModalDialogProperties.ButtonStyles.PRIMARY_FILLED_NEGATIVE_OUTLINE,
                model.get(ModalDialogProperties.BUTTON_STYLES));
    }

    private void verifyDismissed() {
        assertNull(mMockModalDialogManager.getDialogModel());
        assertEquals(-1, mMockModalDialogManager.getDialogType());
    }

    @Test
    public void testDialog_RegularMode() {
        final boolean isIncognito = false;
        when(mIncognitoTabModelMock.getCount()).thenReturn(0);
        CloseAllTabsDialog.show(
                mContext,
                this::getModalDialogManager,
                mTabModelSelectorMock,
                (closePinnedTabs) -> {
                    mRunnableCalled = true;
                },
                isIncognito);
        verifyModel(isIncognito);
        HistogramWatcher histograms =
                HistogramWatcher.newBuilder()
                        .expectNoRecords("Tab.CloseAllTabsDialog.ClosedAllTabs.Incognito")
                        .expectBooleanRecord(
                                "Tab.CloseAllTabsDialog.ClosedAllTabs.NonIncognito", true)
                        .build();

        mMockModalDialogManager.simulateButtonClick(ModalDialogProperties.ButtonType.POSITIVE);
        assertTrue(mRunnableCalled);
        verifyDismissed();
        histograms.assertExpected();
    }

    @Test
    public void testDialog_RegularMode_OneIncognitoTab() {
        final boolean isIncognito = false;
        when(mIncognitoTabModelMock.getCount()).thenReturn(1);
        CloseAllTabsDialog.show(
                mContext,
                this::getModalDialogManager,
                mTabModelSelectorMock,
                (closePinnedTabs) -> {
                    mRunnableCalled = true;
                },
                isIncognito);
        verifyModel(isIncognito);
        HistogramWatcher histograms =
                HistogramWatcher.newBuilder()
                        .expectNoRecords("Tab.CloseAllTabsDialog.ClosedAllTabs.Incognito")
                        .expectBooleanRecord(
                                "Tab.CloseAllTabsDialog.ClosedAllTabs.NonIncognito", true)
                        .build();

        mMockModalDialogManager.simulateButtonClick(ModalDialogProperties.ButtonType.POSITIVE);
        assertTrue(mRunnableCalled);
        verifyDismissed();
        histograms.assertExpected();
    }

    @Test
    public void testDialog_RegularMode_TwoIncognitoTabs() {
        final boolean isIncognito = false;
        when(mIncognitoTabModelMock.getCount()).thenReturn(2);
        CloseAllTabsDialog.show(
                mContext,
                this::getModalDialogManager,
                mTabModelSelectorMock,
                (closePinnedTabs) -> {
                    mRunnableCalled = true;
                },
                isIncognito);
        verifyModel(isIncognito);
        HistogramWatcher histograms =
                HistogramWatcher.newBuilder()
                        .expectNoRecords("Tab.CloseAllTabsDialog.ClosedAllTabs.Incognito")
                        .expectBooleanRecord(
                                "Tab.CloseAllTabsDialog.ClosedAllTabs.NonIncognito", true)
                        .build();

        mMockModalDialogManager.simulateButtonClick(ModalDialogProperties.ButtonType.POSITIVE);
        assertTrue(mRunnableCalled);
        verifyDismissed();
        histograms.assertExpected();
    }

    @Test
    public void testDialog_IncognitoMode() {
        final boolean isIncognito = true;
        when(mIncognitoTabModelMock.getCount()).thenReturn(1);
        CloseAllTabsDialog.show(
                mContext,
                this::getModalDialogManager,
                mTabModelSelectorMock,
                (closePinnedTabs) -> {
                    mRunnableCalled = true;
                },
                isIncognito);
        verifyModel(isIncognito);
        HistogramWatcher histograms =
                HistogramWatcher.newBuilder()
                        .expectNoRecords("Tab.CloseAllTabsDialog.ClosedAllTabs.NonIncognito")
                        .expectBooleanRecord("Tab.CloseAllTabsDialog.ClosedAllTabs.Incognito", true)
                        .build();

        mMockModalDialogManager.simulateButtonClick(ModalDialogProperties.ButtonType.POSITIVE);
        assertTrue(mRunnableCalled);
        verifyDismissed();
        histograms.assertExpected();
    }

    @Test
    public void testDismissButton() {
        final boolean isIncognito = true;
        when(mIncognitoTabModelMock.getCount()).thenReturn(1);
        CloseAllTabsDialog.show(
                mContext,
                this::getModalDialogManager,
                mTabModelSelectorMock,
                (closePinnedTabs) -> {
                    mRunnableCalled = true;
                },
                isIncognito);
        verifyModel(isIncognito);
        HistogramWatcher histograms =
                HistogramWatcher.newBuilder()
                        .expectBooleanRecord(
                                "Tab.CloseAllTabsDialog.ClosedAllTabs.Incognito", false)
                        .expectNoRecords("Tab.CloseAllTabsDialog.ClosedAllTabs.NonIncognito")
                        .build();

        mMockModalDialogManager.simulateButtonClick(ModalDialogProperties.ButtonType.NEGATIVE);
        assertFalse(mRunnableCalled);
        verifyDismissed();
        histograms.assertExpected();
    }

    @Test
    public void testDismissNoButton() {
        final boolean isIncognito = false;
        when(mIncognitoTabModelMock.getCount()).thenReturn(0);
        CloseAllTabsDialog.show(
                mContext,
                this::getModalDialogManager,
                mTabModelSelectorMock,
                (closePinnedTabs) -> {
                    mRunnableCalled = true;
                },
                isIncognito);
        verifyModel(isIncognito);
        HistogramWatcher histograms =
                HistogramWatcher.newBuilder()
                        .expectNoRecords("Tab.CloseAllTabsDialog.ClosedAllTabs.Incognito")
                        .expectBooleanRecord(
                                "Tab.CloseAllTabsDialog.ClosedAllTabs.NonIncognito", false)
                        .build();

        mMockModalDialogManager.dismissDialog(
                mMockModalDialogManager.getDialogModel(),
                DialogDismissalCause.NAVIGATE_BACK_OR_TOUCH_OUTSIDE);
        assertFalse(mRunnableCalled);
        verifyDismissed();
        histograms.assertExpected();
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_CLOSE_ALL_TABS_RETAINS_PINNED_TABS)
    public void testDialog_WithPinnedTabs_CheckboxUncheckedByDefault_RetainsPinnedTabs() {
        when(mIncognitoTabModelMock.getCount()).thenReturn(0);
        when(mTabModelMock.getCount()).thenReturn(4);
        when(mTabModelMock.getPinnedTabsCount()).thenReturn(2);

        AtomicReference<Boolean> callbackResult = new AtomicReference<>();

        CloseAllTabsDialog.show(
                mContext,
                this::getModalDialogManager,
                mTabModelSelectorMock,
                (closePinnedTabs) -> callbackResult.set(closePinnedTabs),
                /* isIncognitoOnly= */ false);

        PropertyModel model = mMockModalDialogManager.getDialogModel();
        assertNotNull(model);
        assertEquals(
                mContext.getString(R.string.close_all_tabs_dialog_also_close_pinned_tabs),
                model.get(ModalDialogProperties.CHECKBOX_TEXT));
        assertFalse(model.get(ModalDialogProperties.CHECKBOX_CHECKED));
        assertFalse(model.get(ModalDialogProperties.POSITIVE_BUTTON_DISABLED));

        HistogramWatcher confirmHistograms =
                HistogramWatcher.newBuilder()
                        .expectBooleanRecord("Tab.CloseAllTabsDialog.AlsoClosePinnedTabs", false)
                        .expectBooleanRecord(
                                "Tab.CloseAllTabsDialog.ClosedAllTabs.NonIncognito", true)
                        .build();

        mMockModalDialogManager.simulateButtonClick(ModalDialogProperties.ButtonType.POSITIVE);
        assertFalse(callbackResult.get());
        verifyDismissed();
        confirmHistograms.assertExpected();
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_CLOSE_ALL_TABS_RETAINS_PINNED_TABS)
    public void testDialog_WithPinnedTabs_CheckboxChecked_ClosesPinnedTabs() {
        when(mIncognitoTabModelMock.getCount()).thenReturn(0);
        when(mTabModelMock.getCount()).thenReturn(4);
        when(mTabModelMock.getPinnedTabsCount()).thenReturn(3);

        AtomicReference<Boolean> callbackResult = new AtomicReference<>();
        CloseAllTabsDialog.show(
                mContext,
                this::getModalDialogManager,
                mTabModelSelectorMock,
                (closePinnedTabs) -> callbackResult.set(closePinnedTabs),
                /* isIncognitoOnly= */ false);

        PropertyModel model = mMockModalDialogManager.getDialogModel();
        assertNotNull(model);
        mMockModalDialogManager.simulateCheckboxToggle(true);

        HistogramWatcher confirmHistograms =
                HistogramWatcher.newBuilder()
                        .expectBooleanRecord("Tab.CloseAllTabsDialog.AlsoClosePinnedTabs", true)
                        .expectBooleanRecord(
                                "Tab.CloseAllTabsDialog.ClosedAllTabs.NonIncognito", true)
                        .build();

        mMockModalDialogManager.simulateButtonClick(ModalDialogProperties.ButtonType.POSITIVE);
        assertTrue(callbackResult.get());
        verifyDismissed();
        confirmHistograms.assertExpected();
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_CLOSE_ALL_TABS_RETAINS_PINNED_TABS)
    public void testDialog_OnlyPinnedTabs_PositiveButtonDisabledUntilCheckboxChecked() {
        when(mIncognitoTabModelMock.getCount()).thenReturn(0);
        when(mTabModelMock.getCount()).thenReturn(2);
        when(mTabModelMock.getPinnedTabsCount()).thenReturn(2);

        CloseAllTabsDialog.show(
                mContext,
                this::getModalDialogManager,
                mTabModelSelectorMock,
                (closePinnedTabs) -> {},
                /* isIncognitoOnly= */ false);

        PropertyModel model = mMockModalDialogManager.getDialogModel();
        assertNotNull(model);
        assertTrue(model.get(ModalDialogProperties.POSITIVE_BUTTON_DISABLED));

        mMockModalDialogManager.simulateCheckboxToggle(true);
        assertFalse(model.get(ModalDialogProperties.POSITIVE_BUTTON_DISABLED));

        mMockModalDialogManager.simulateCheckboxToggle(false);
        assertTrue(model.get(ModalDialogProperties.POSITIVE_BUTTON_DISABLED));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.ANDROID_CLOSE_ALL_TABS_RETAINS_PINNED_TABS)
    public void testDialog_NoPinnedTabs_CheckboxNotShown() {
        when(mIncognitoTabModelMock.getCount()).thenReturn(0);
        when(mTabModelMock.getPinnedTabsCount()).thenReturn(0);

        CloseAllTabsDialog.show(
                mContext,
                this::getModalDialogManager,
                mTabModelSelectorMock,
                (closePinnedTabs) -> {},
                /* isIncognitoOnly= */ false);

        PropertyModel model = mMockModalDialogManager.getDialogModel();
        assertNotNull(model);
        assertNull(model.get(ModalDialogProperties.CHECKBOX_TEXT));
    }

    @Test
    @DisableFeatures(ChromeFeatureList.ANDROID_CLOSE_ALL_TABS_RETAINS_PINNED_TABS)
    public void testDialog_FlagDisabled_CheckboxNotShown() {
        when(mIncognitoTabModelMock.getCount()).thenReturn(0);
        when(mTabModelMock.getPinnedTabsCount()).thenReturn(2);

        CloseAllTabsDialog.show(
                mContext,
                this::getModalDialogManager,
                mTabModelSelectorMock,
                (closePinnedTabs) -> mRunnableCalled = true,
                /* isIncognitoOnly= */ false);

        PropertyModel model = mMockModalDialogManager.getDialogModel();
        assertNotNull(model);
        assertNull(model.get(ModalDialogProperties.CHECKBOX_TEXT));

        HistogramWatcher watcher =
                HistogramWatcher.newBuilder()
                        .expectNoRecords("Tab.CloseAllTabsDialog.AlsoClosePinnedTabs")
                        .expectBooleanRecord(
                                "Tab.CloseAllTabsDialog.ClosedAllTabs.NonIncognito", true)
                        .build();

        mMockModalDialogManager.simulateButtonClick(ModalDialogProperties.ButtonType.POSITIVE);
        assertTrue(mRunnableCalled);
        verifyDismissed();
        watcher.assertExpected();
    }
}
