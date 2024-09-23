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
import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.tab_ui.R;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * Tests fo the close all tabs dialog to confirm the close all tabs action.
 *
 * <p>This test assumes that the Modal Dialog UI component is well tested responds correctly to user
 * inputs. It only tests the logic and properties of the dialog.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
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

    private Context mContext;
    private MockModalDialogManager mMockModalDialogManager;
    private boolean mRunnableCalled;

    @Mock private TabModelSelector mTabModelSelectorMock;
    @Mock private TabModel mTabModelMock;
    @Mock private TabModel mIncognitoTabModelMock;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        when(mTabModelMock.isIncognito()).thenReturn(false);
        when(mIncognitoTabModelMock.isIncognito()).thenReturn(true);
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
                CloseAllTabsDialog.getDialogDescriptionString(mContext, mTabModelSelectorMock),
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

    private void setUpCurrentModelAndIncognitoCount(boolean incognito, int incognitoTabCount) {
        when(mTabModelSelectorMock.getCurrentModel())
                .thenReturn(incognito ? mIncognitoTabModelMock : mTabModelMock);
        when(mIncognitoTabModelMock.getCount()).thenReturn(incognitoTabCount);
    }

    @Test
    @SmallTest
    public void testDialog_RegularMode() {
        final boolean isIncognito = false;
        setUpCurrentModelAndIncognitoCount(isIncognito, 0);
        CloseAllTabsDialog.show(
                mContext,
                this::getModalDialogManager,
                mTabModelSelectorMock,
                () -> {
                    mRunnableCalled = true;
                });
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
    @SmallTest
    public void testDialog_RegularMode_OneIncognitoTab() {
        final boolean isIncognito = false;
        setUpCurrentModelAndIncognitoCount(isIncognito, 1);
        CloseAllTabsDialog.show(
                mContext,
                this::getModalDialogManager,
                mTabModelSelectorMock,
                () -> {
                    mRunnableCalled = true;
                });
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
    @SmallTest
    public void testDialog_RegularMode_TwoIncognitoTabs() {
        final boolean isIncognito = false;
        setUpCurrentModelAndIncognitoCount(isIncognito, 2);
        CloseAllTabsDialog.show(
                mContext,
                this::getModalDialogManager,
                mTabModelSelectorMock,
                () -> {
                    mRunnableCalled = true;
                });
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
    @SmallTest
    public void testDialog_IncognitoMode() {
        final boolean isIncognito = true;
        setUpCurrentModelAndIncognitoCount(isIncognito, 1);
        CloseAllTabsDialog.show(
                mContext,
                this::getModalDialogManager,
                mTabModelSelectorMock,
                () -> {
                    mRunnableCalled = true;
                });
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
    @SmallTest
    public void testDismissButton() {
        final boolean isIncognito = true;
        setUpCurrentModelAndIncognitoCount(isIncognito, 1);
        CloseAllTabsDialog.show(
                mContext,
                this::getModalDialogManager,
                mTabModelSelectorMock,
                () -> {
                    mRunnableCalled = true;
                });
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
    @SmallTest
    public void testDismissNoButton() {
        final boolean isIncognito = false;
        setUpCurrentModelAndIncognitoCount(isIncognito, 0);
        CloseAllTabsDialog.show(
                mContext,
                this::getModalDialogManager,
                mTabModelSelectorMock,
                () -> {
                    mRunnableCalled = true;
                });
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
}
