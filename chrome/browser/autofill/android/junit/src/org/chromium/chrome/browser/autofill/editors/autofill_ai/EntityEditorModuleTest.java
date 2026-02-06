// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.editors.autofill_ai;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import android.app.Activity;
import android.view.View;

import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.MockitoAnnotations;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.mockito.quality.Strictness;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.autofill.editors.common.EditorDialogToolbar;
import org.chromium.components.autofill.autofill_ai.EntityInstance;
import org.chromium.components.autofill.autofill_ai.EntityType;
import org.chromium.components.autofill.autofill_ai.EntityTypeName;
import org.chromium.components.autofill.autofill_ai.RecordType;
import org.chromium.ui.base.TestActivity;

import java.time.LocalDate;
import java.time.ZoneId;

@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@Batch(Batch.UNIT_TESTS)
public class EntityEditorModuleTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule().strictness(Strictness.STRICT_STUBS);

    private Activity mActivity;
    private EntityEditorCoordinator mCoordinator;
    private View mContainerView;

    private EntityInstance mEntityInstance;

    @Before
    public void setUp() {
        MockitoAnnotations.openMocks(this);
        mActivity = Robolectric.setupActivity(TestActivity.class);
        mCoordinator = new EntityEditorCoordinator(mActivity);
        mContainerView = mCoordinator.getEntityEditorViewForTest().getContainerView();

        EntityType entityType =
                new EntityType(
                        /* typeName= */ EntityTypeName.PASSPORT,
                        /* isReadOnly= */ false,
                        /* typeNameAsString= */ "Passport",
                        /* addEntityTypeString= */ "Add passport",
                        /* editEntityTypeString= */ "Edit passport",
                        /* deleteEntityTypeString= */ "Delete passport");
        mEntityInstance =
                new EntityInstance.Builder(entityType)
                        .setGUID("guid")
                        .setRecordType(RecordType.LOCAL)
                        .setModifiedDate(LocalDate.now(ZoneId.systemDefault()))
                        .setUseCount(0)
                        .build();
    }

    @Test
    @SmallTest
    public void testShowEditorDialog() {
        mCoordinator.showEditorDialog(mEntityInstance);
        EditorDialogToolbar toolbar = mContainerView.findViewById(R.id.action_bar);
        assertEquals(mEntityInstance.getEntityType().getAddEntityTypeString(), toolbar.getTitle());
        assertTrue(mCoordinator.getEditorModelForTest().get(EntityEditorProperties.VISIBLE));
    }

    @Test
    @SmallTest
    public void testClickDoneButton() {
        mCoordinator.showEditorDialog(mEntityInstance);
        mContainerView.findViewById(R.id.editor_dialog_done_button).performClick();
        assertFalse(mCoordinator.getEditorModelForTest().get(EntityEditorProperties.VISIBLE));
    }

    @Test
    @SmallTest
    public void testClickCancelButton() {
        mCoordinator.showEditorDialog(mEntityInstance);
        mContainerView.findViewById(R.id.payments_edit_cancel_button).performClick();
        assertFalse(mCoordinator.getEditorModelForTest().get(EntityEditorProperties.VISIBLE));
    }
}
