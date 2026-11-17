// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.history;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertSame;
import static org.junit.Assert.assertTrue;

import androidx.test.core.app.ApplicationProvider;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.history.CriticalActionsProperties.ItemType;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.ArrayList;
import java.util.List;

/** Unit tests for {@link CriticalActionsMediator}. */
@RunWith(BaseRobolectricTestRunner.class)
@EnableFeatures(ChromeFeatureList.CRITICAL_ACTION_HISTORY)
public class CriticalActionsMediatorTest {
    private static final List<String> LABELS = List.of("Filled a form", "Used a password");

    private PropertyModel mModel;
    private ModelList mModelList;
    private CriticalActionsMediator mMediator;
    private final List<Boolean> mExpansionChanges = new ArrayList<>();

    @Before
    public void setUp() {
        mModel = new PropertyModel(CriticalActionsProperties.ALL_KEYS);
        mModelList = new ModelList();
        mMediator =
                new CriticalActionsMediator(
                        ApplicationProvider.getApplicationContext(), mModel, mModelList);
    }

    private void attach(List<String> labels, boolean expanded, boolean selectionActive) {
        mMediator.attachToHistoryItem(labels, expanded, selectionActive, mExpansionChanges::add);
    }

    private void assertHidden() {
        assertFalse(mModel.get(CriticalActionsProperties.EXPAND_ICON_VISIBLE));
        assertNull(mModel.get(CriticalActionsProperties.EXPAND_ICON_CONTENT_DESCRIPTION));
        assertFalse(mModel.get(CriticalActionsProperties.PANEL_VISIBLE));
        assertNull(mModel.get(CriticalActionsProperties.EXPAND_ICON_CLICK_HANDLER));
        assertNull(mModel.get(CriticalActionsProperties.EXPAND_ICON_CLICK_ACCESSIBILITY_LABEL));
        assertEquals(0, mModelList.size());
    }

    @Test
    public void testAttach_Collapsed() {
        attach(LABELS, /* expanded= */ false, /* selectionActive= */ false);

        assertTrue(mModel.get(CriticalActionsProperties.EXPAND_ICON_VISIBLE));
        assertEquals(
                "2 key browsing actions",
                mModel.get(CriticalActionsProperties.EXPAND_ICON_CONTENT_DESCRIPTION));
        assertEquals(0f, mModel.get(CriticalActionsProperties.EXPAND_ICON_ROTATION), 0f);
        assertFalse(mModel.get(CriticalActionsProperties.PANEL_VISIBLE));
        assertNotNull(mModel.get(CriticalActionsProperties.EXPAND_ICON_CLICK_HANDLER));
        assertEquals(
                "Expand section",
                mModel.get(CriticalActionsProperties.EXPAND_ICON_CLICK_ACCESSIBILITY_LABEL));

        assertEquals(2, mModelList.size());
        for (int i = 0; i < LABELS.size(); i++) {
            assertEquals(ItemType.ACTION, mModelList.get(i).type);
            assertEquals(
                    LABELS.get(i), mModelList.get(i).model.get(CriticalActionsProperties.LABEL));
        }
    }

    @Test
    public void testAttach_Expanded() {
        attach(LABELS, /* expanded= */ true, /* selectionActive= */ false);

        assertTrue(mModel.get(CriticalActionsProperties.PANEL_VISIBLE));
        assertEquals(180f, mModel.get(CriticalActionsProperties.EXPAND_ICON_ROTATION), 0f);
        assertEquals(
                "Collapse section",
                mModel.get(CriticalActionsProperties.EXPAND_ICON_CLICK_ACCESSIBILITY_LABEL));
    }

    @Test
    public void testAttach_NoActions() {
        attach(List.of(), /* expanded= */ true, /* selectionActive= */ false);
        assertHidden();
    }

    @Test
    public void testReattach_ClearsPreviousItem() {
        attach(LABELS, /* expanded= */ true, /* selectionActive= */ false);
        attach(List.of(), /* expanded= */ false, /* selectionActive= */ false);
        assertHidden();
    }

    @Test
    public void testToggle() {
        attach(LABELS, /* expanded= */ false, /* selectionActive= */ false);
        Runnable expandIconClickHandler =
                mModel.get(CriticalActionsProperties.EXPAND_ICON_CLICK_HANDLER);
        assertNotNull(expandIconClickHandler);

        expandIconClickHandler.run();
        assertEquals(List.of(true), mExpansionChanges);
        assertTrue(mModel.get(CriticalActionsProperties.PANEL_VISIBLE));
        assertEquals(180f, mModel.get(CriticalActionsProperties.EXPAND_ICON_ROTATION), 0f);
        assertEquals(
                "Collapse section",
                mModel.get(CriticalActionsProperties.EXPAND_ICON_CLICK_ACCESSIBILITY_LABEL));

        expandIconClickHandler.run();
        assertEquals(List.of(true, false), mExpansionChanges);
        assertFalse(mModel.get(CriticalActionsProperties.PANEL_VISIBLE));
        assertEquals(0f, mModel.get(CriticalActionsProperties.EXPAND_ICON_ROTATION), 0f);
        assertEquals(
                "Expand section",
                mModel.get(CriticalActionsProperties.EXPAND_ICON_CLICK_ACCESSIBILITY_LABEL));
    }

    @Test
    public void testExpandIconClickHandlerUnchangedByToggleAndSelection() {
        attach(LABELS, /* expanded= */ false, /* selectionActive= */ false);
        Runnable expandIconClickHandler =
                mModel.get(CriticalActionsProperties.EXPAND_ICON_CLICK_HANDLER);
        assertNotNull(expandIconClickHandler);

        expandIconClickHandler.run();
        mMediator.setSelectionActive(true);
        assertSame(
                expandIconClickHandler,
                mModel.get(CriticalActionsProperties.EXPAND_ICON_CLICK_HANDLER));
    }

    @Test
    public void testDestroy() {
        attach(LABELS, /* expanded= */ false, /* selectionActive= */ false);

        mMediator.destroy();
        assertNull(mModel.get(CriticalActionsProperties.EXPAND_ICON_CLICK_HANDLER));
        assertNull(mModel.get(CriticalActionsProperties.EXPAND_ICON_CLICK_ACCESSIBILITY_LABEL));
    }

    @Test
    public void testSelectionActive() {
        attach(LABELS, /* expanded= */ true, /* selectionActive= */ true);
        assertFalse(mModel.get(CriticalActionsProperties.EXPAND_ICON_VISIBLE));
        assertNull(mModel.get(CriticalActionsProperties.EXPAND_ICON_CLICK_ACCESSIBILITY_LABEL));
        // The panel stays expanded while selecting items.
        assertTrue(mModel.get(CriticalActionsProperties.PANEL_VISIBLE));

        mMediator.setSelectionActive(false);
        assertTrue(mModel.get(CriticalActionsProperties.EXPAND_ICON_VISIBLE));
        assertEquals(
                "Collapse section",
                mModel.get(CriticalActionsProperties.EXPAND_ICON_CLICK_ACCESSIBILITY_LABEL));

        mMediator.setSelectionActive(true);
        assertFalse(mModel.get(CriticalActionsProperties.EXPAND_ICON_VISIBLE));
        assertNull(mModel.get(CriticalActionsProperties.EXPAND_ICON_CLICK_ACCESSIBILITY_LABEL));
    }

    @Test
    @DisableFeatures(ChromeFeatureList.CRITICAL_ACTION_HISTORY)
    public void testFeatureDisabled() {
        attach(LABELS, /* expanded= */ true, /* selectionActive= */ false);
        assertHidden();
        assertFalse(CriticalActionsMediator.shouldShowCriticalActions(LABELS));
    }
}
