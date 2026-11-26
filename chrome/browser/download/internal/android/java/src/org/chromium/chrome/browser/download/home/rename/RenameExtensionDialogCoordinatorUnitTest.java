// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download.home.rename;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertSame;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoInteractions;
import static org.mockito.Mockito.verifyNoMoreInteractions;

import android.content.Context;
import android.content.ContextWrapper;
import android.widget.ScrollView;

import androidx.test.core.app.ApplicationProvider;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.Callback;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.download.internal.R;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogManager.ModalDialogType;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modelutil.PropertyModel;

/** Unit tests for {@link RenameExtensionDialogCoordinator}. */
@RunWith(BaseRobolectricTestRunner.class)
public class RenameExtensionDialogCoordinatorUnitTest {
    private static class TrackingContext extends ContextWrapper {
        private int mLayoutInflaterLookups;

        TrackingContext(Context base) {
            super(base);
        }

        @Override
        public Object getSystemService(String name) {
            if (Context.LAYOUT_INFLATER_SERVICE.equals(name)) {
                mLayoutInflaterLookups++;
            }
            return super.getSystemService(name);
        }

        int getLayoutInflaterLookupCount() {
            return mLayoutInflaterLookups;
        }
    }

    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private ModalDialogManager mModalDialogManager;
    @Mock private Callback<Boolean> mOnClickCallback;
    @Mock private Callback<Integer> mOnDismissCallback;
    @Captor private ArgumentCaptor<PropertyModel> mModelCaptor;

    private TrackingContext mContext;

    @Before
    public void setUp() {
        mContext = new TrackingContext(ApplicationProvider.getApplicationContext());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.CLANK_STARTUP_TAB_OPTIMIZATIONS)
    public void testConstructor_defersInflationAndModelCreation() {
        createCoordinator();

        assertEquals(0, mContext.getLayoutInflaterLookupCount());
        verifyNoInteractions(mModalDialogManager);
    }

    @Test
    @DisableFeatures(ChromeFeatureList.CLANK_STARTUP_TAB_OPTIMIZATIONS)
    public void testConstructor_inflatesEagerlyWhenFeatureDisabled() {
        RenameExtensionDialogCoordinator coordinator = createCoordinator();

        assertEquals(1, mContext.getLayoutInflaterLookupCount());
        verifyNoInteractions(mModalDialogManager);

        coordinator.showDialog();

        assertEquals(1, mContext.getLayoutInflaterLookupCount());
        verify(mModalDialogManager)
                .showDialog(mModelCaptor.capture(), eq(ModalDialogType.APP), eq(true));
        assertDialogModelProperties(mModelCaptor.getValue());
    }

    @Test
    @DisableFeatures(ChromeFeatureList.CLANK_STARTUP_TAB_OPTIMIZATIONS)
    public void testDismissAndDestroyWithNullModalDialogManager_doesNotThrow() {
        RenameExtensionDialogCoordinator coordinator =
                new RenameExtensionDialogCoordinator(
                        mContext, null, mOnClickCallback, mOnDismissCallback);

        assertEquals(1, mContext.getLayoutInflaterLookupCount());
        coordinator.showDialog();
        coordinator.dismissDialog(DialogDismissalCause.NEGATIVE_BUTTON_CLICKED);
        coordinator.destroy();
    }

    @Test
    @EnableFeatures(ChromeFeatureList.CLANK_STARTUP_TAB_OPTIMIZATIONS)
    public void testDismissAndDestroyWithoutShowDialog_areNoOps() {
        RenameExtensionDialogCoordinator coordinator = createCoordinator();

        coordinator.dismissDialog(DialogDismissalCause.NEGATIVE_BUTTON_CLICKED);
        coordinator.destroy();

        assertEquals(0, mContext.getLayoutInflaterLookupCount());
        verifyNoInteractions(mModalDialogManager);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.CLANK_STARTUP_TAB_OPTIMIZATIONS)
    public void testShowDialog_inflatesCustomViewOnceAndReusesModel() {
        RenameExtensionDialogCoordinator coordinator = createCoordinator();

        coordinator.showDialog();

        assertEquals(1, mContext.getLayoutInflaterLookupCount());
        verify(mModalDialogManager)
                .showDialog(mModelCaptor.capture(), eq(ModalDialogType.APP), eq(true));

        PropertyModel firstModel = mModelCaptor.getValue();
        assertDialogModelProperties(firstModel);

        coordinator.showDialog();

        assertEquals(1, mContext.getLayoutInflaterLookupCount());
        verify(mModalDialogManager, times(2))
                .showDialog(mModelCaptor.capture(), eq(ModalDialogType.APP), eq(true));
        assertSame(firstModel, mModelCaptor.getValue());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.CLANK_STARTUP_TAB_OPTIMIZATIONS)
    public void testDismissAndDestroyAfterShowDialog_dismissesModel() {
        RenameExtensionDialogCoordinator coordinator = createCoordinator();

        coordinator.showDialog();
        verify(mModalDialogManager)
                .showDialog(mModelCaptor.capture(), eq(ModalDialogType.APP), eq(true));
        PropertyModel model = mModelCaptor.getValue();

        coordinator.dismissDialog(DialogDismissalCause.NEGATIVE_BUTTON_CLICKED);
        verify(mModalDialogManager)
                .dismissDialog(model, DialogDismissalCause.NEGATIVE_BUTTON_CLICKED);

        coordinator.destroy();
        verify(mModalDialogManager).dismissDialog(model, DialogDismissalCause.ACTIVITY_DESTROYED);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.CLANK_STARTUP_TAB_OPTIMIZATIONS)
    public void testControllerCallbacks_forwardClickAndDismissEvents() {
        RenameExtensionDialogCoordinator coordinator = createCoordinator();

        coordinator.showDialog();
        verify(mModalDialogManager)
                .showDialog(mModelCaptor.capture(), eq(ModalDialogType.APP), eq(true));
        PropertyModel model = mModelCaptor.getValue();
        ModalDialogProperties.Controller controller = model.get(ModalDialogProperties.CONTROLLER);

        controller.onClick(model, ModalDialogProperties.ButtonType.POSITIVE);
        verify(mOnClickCallback).onResult(true);

        controller.onClick(model, ModalDialogProperties.ButtonType.NEGATIVE);
        verify(mOnClickCallback).onResult(false);

        controller.onClick(model, ModalDialogProperties.ButtonType.POSITIVE_EPHEMERAL);
        verifyNoMoreInteractions(mOnClickCallback);

        controller.onDismiss(model, DialogDismissalCause.NAVIGATE_BACK_OR_TOUCH_OUTSIDE);
        verify(mOnDismissCallback).onResult(DialogDismissalCause.NAVIGATE_BACK_OR_TOUCH_OUTSIDE);
    }

    private RenameExtensionDialogCoordinator createCoordinator() {
        return new RenameExtensionDialogCoordinator(
                mContext, mModalDialogManager, mOnClickCallback, mOnDismissCallback);
    }

    private void assertDialogModelProperties(PropertyModel model) {
        assertNotNull(model);
        assertEquals(
                mContext.getString(R.string.rename_extension_confirmation),
                model.get(ModalDialogProperties.TITLE));
        assertTrue(model.get(ModalDialogProperties.CUSTOM_VIEW) instanceof ScrollView);
        assertEquals(
                mContext.getString(R.string.confirm),
                model.get(ModalDialogProperties.POSITIVE_BUTTON_TEXT));
        assertEquals(
                mContext.getString(R.string.cancel),
                model.get(ModalDialogProperties.NEGATIVE_BUTTON_TEXT));
    }
}
