// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.editors.autofill_ai;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.junit.Assert.fail;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import static org.chromium.chrome.browser.autofill.editors.common.EditorComponentsProperties.ItemType.NOTICE;
import static org.chromium.chrome.browser.autofill.editors.common.EditorComponentsProperties.NoticeProperties.IMPORTANT_FOR_ACCESSIBILITY;
import static org.chromium.chrome.browser.autofill.editors.common.EditorComponentsProperties.NoticeProperties.NOTICE_TEXT;
import static org.chromium.chrome.browser.autofill.editors.common.EditorComponentsProperties.NoticeProperties.SHOW_BACKGROUND;

import android.app.Activity;
import android.view.View;

import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.mockito.quality.Strictness;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.autofill.editors.autofill_ai.EntityEditorCoordinator.Delegate;
import org.chromium.chrome.browser.autofill.editors.common.EditorComponentsProperties.EditorItem;
import org.chromium.chrome.browser.autofill.editors.common.EditorDialogToolbar;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.components.autofill.autofill_ai.EntityInstance;
import org.chromium.components.autofill.autofill_ai.EntityType;
import org.chromium.components.autofill.autofill_ai.EntityTypeName;
import org.chromium.components.autofill.autofill_ai.RecordType;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.google_apis.gaia.GaiaId;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.modelutil.ListModel;
import org.chromium.ui.modelutil.PropertyModel;

import java.time.LocalDate;
import java.time.ZoneId;
import java.util.Collections;

@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@Batch(Batch.UNIT_TESTS)
public class EntityEditorModuleTest {
    private static final String USER_EMAIL = "example@gmail.com";
    private static final EntityType PASSPORT_TYPE =
            new EntityType(
                    /* typeName= */ EntityTypeName.PASSPORT,
                    /* isReadOnly= */ false,
                    /* typeNameAsString= */ "Passport",
                    /* addEntityTypeString= */ "Add passport",
                    /* editEntityTypeString= */ "Edit passport",
                    /* deleteEntityTypeString= */ "Delete passport",
                    /* attributeTypes= */ Collections.emptyList());

    private static final EntityInstance LOCAL_PASSPORT =
            new EntityInstance.Builder(PASSPORT_TYPE)
                    .setGUID("guid")
                    .setRecordType(RecordType.LOCAL)
                    .setModifiedDate(LocalDate.now(ZoneId.systemDefault()))
                    .setUseCount(0)
                    .build();

    private static final EntityInstance WALLET_PASSPORT =
            new EntityInstance.Builder(PASSPORT_TYPE)
                    .setGUID("guid")
                    .setRecordType(RecordType.SERVER_WALLET)
                    .setModifiedDate(LocalDate.now(ZoneId.systemDefault()))
                    .setUseCount(0)
                    .build();

    private final CoreAccountInfo mAccountInfo =
            CoreAccountInfo.createFromEmailAndGaiaId(USER_EMAIL, new GaiaId("gaia_id"));

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule().strictness(Strictness.STRICT_STUBS);
    @Mock private Delegate mDelegate;
    @Mock private Profile mProfile;
    @Mock private IdentityManager mIdentityManager;

    private Activity mActivity;
    private EntityEditorCoordinator mCoordinator;
    private View mContainerView;

    @Before
    public void setUp() {
        MockitoAnnotations.openMocks(this);

        IdentityServicesProvider.setIdentityManagerForTesting(mIdentityManager);

        mActivity = Robolectric.setupActivity(TestActivity.class);
        mCoordinator = new EntityEditorCoordinator(mActivity, mDelegate, mProfile);
        mContainerView = mCoordinator.getEntityEditorViewForTest().getContainerView();
    }

    @Test
    @SmallTest
    public void testShowEditorDialog() {
        mCoordinator.showEditorDialog(LOCAL_PASSPORT);
        EditorDialogToolbar toolbar = mContainerView.findViewById(R.id.action_bar);
        assertEquals(PASSPORT_TYPE.getAddEntityTypeString(), toolbar.getTitle());
        assertTrue(mCoordinator.getEditorModelForTest().get(EntityEditorProperties.VISIBLE));
    }

    @Test
    @SmallTest
    public void testClickDoneButton() {
        mCoordinator.showEditorDialog(LOCAL_PASSPORT);
        mContainerView.findViewById(R.id.editor_dialog_done_button).performClick();
        assertFalse(mCoordinator.getEditorModelForTest().get(EntityEditorProperties.VISIBLE));
    }

    @Test
    @SmallTest
    public void testClickCancelButton() {
        mCoordinator.showEditorDialog(LOCAL_PASSPORT);
        mContainerView.findViewById(R.id.payments_edit_cancel_button).performClick();
        assertFalse(mCoordinator.getEditorModelForTest().get(EntityEditorProperties.VISIBLE));
    }

    @Test
    @SmallTest
    public void testDeleteLocalEntity() {
        mCoordinator.showEditorDialog(LOCAL_PASSPORT);
        PropertyModel model = mCoordinator.getEditorModelForTest();
        assertTrue(model.get(EntityEditorProperties.ALLOW_DELETE));
        assertEquals(
                model.get(EntityEditorProperties.DELETE_CONFIRMATION_TITLE),
                PASSPORT_TYPE.getDeleteEntityTypeString());
        assertEquals(
                model.get(EntityEditorProperties.DELETE_CONFIRMATION_TEXT),
                mActivity.getString(
                        R.string.autofill_ai_entity_editor_delete_local_entity_dialog_text));
        assertEquals(
                model.get(EntityEditorProperties.DELETE_CONFIRMATION_PRIMARY_BUTTON_TEXT_ID),
                R.string.autofill_delete_suggestion_button);

        model.get(EntityEditorProperties.DELETE_RUNNABLE).run();
        verify(mDelegate).onDelete(LOCAL_PASSPORT);
    }

    @Test
    @SmallTest
    public void testDeleteWalletEntity() {
        mCoordinator.showEditorDialog(WALLET_PASSPORT);

        PropertyModel model = mCoordinator.getEditorModelForTest();
        assertFalse(model.get(EntityEditorProperties.ALLOW_DELETE));
    }

    @Test
    @SmallTest
    public void testLocalEntitySourceNotice() {
        mCoordinator.showEditorDialog(LOCAL_PASSPORT);

        PropertyModel model = mCoordinator.getEditorModelForTest();
        verifySourceNotice(
                model.get(EntityEditorProperties.EDITOR_FIELDS),
                mActivity.getString(R.string.autofill_ai_local_entity_editor_source_notice));
    }

    @Test
    @SmallTest
    public void testWalletEntitySourceNotice() {
        when(mIdentityManager.getPrimaryAccountInfo(anyInt())).thenReturn(mAccountInfo);
        mCoordinator.showEditorDialog(WALLET_PASSPORT);

        PropertyModel model = mCoordinator.getEditorModelForTest();
        verifySourceNotice(
                model.get(EntityEditorProperties.EDITOR_FIELDS),
                mActivity
                        .getString(R.string.autofill_ai_wallet_entity_editor_source_notice)
                        .replace("$1", USER_EMAIL));
    }

    private void verifySourceNotice(ListModel<EditorItem> editorFields, String expectedNoticeText) {
        for (EditorItem item : editorFields) {
            if (item.type == NOTICE && expectedNoticeText.equals(item.model.get(NOTICE_TEXT))) {
                assertTrue(item.model.get(SHOW_BACKGROUND));
                assertTrue(item.model.get(IMPORTANT_FOR_ACCESSIBILITY));
                return;
            }
        }
        fail("Source notice not found");
    }
}
