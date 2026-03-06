// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.search_engines.settings.dialog;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.when;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.components.search_engines.TemplateUrl;

/** Unit tests for {@link SiteSearchDialogDraft}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class SiteSearchDialogDraftUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private TemplateUrl mTemplateUrl;

    @Test
    public void testCreateForAdd() {
        // Passing null mimics the "Add" state.
        SiteSearchDialogDraft draft = SiteSearchDialogDraft.create(null);

        assertEquals("", draft.getNameInput());
        assertEquals("", draft.getKeywordInput());
        assertEquals("", draft.getUrlInput());
        assertEquals("", draft.getOriginalKeyword());

        // Assert the new isTryingToAdd state
        assertTrue(draft.isTryingToAdd());

        assertFalse(draft.areAllInputsValid());
    }

    @Test
    public void testCreateForEdit() {
        when(mTemplateUrl.getShortName()).thenReturn("My Engine");
        when(mTemplateUrl.getKeyword()).thenReturn("engine.com");
        when(mTemplateUrl.getURL()).thenReturn("https://engine.com/search?q=%s");

        // Passing a TemplateUrl mimics the "Edit" state.
        SiteSearchDialogDraft draft = SiteSearchDialogDraft.create(mTemplateUrl);

        assertEquals("My Engine", draft.getNameInput());
        assertEquals("engine.com", draft.getKeywordInput());
        assertEquals("https://engine.com/search?q=%s", draft.getUrlInput());
        assertEquals("engine.com", draft.getOriginalKeyword());

        // Assert the new isTryingToAdd state
        assertFalse(draft.isTryingToAdd());

        // Default validities for existing items should be true
        assertTrue(draft.areAllInputsValid());
    }

    @Test
    public void testAreAllInputsValid() {
        SiteSearchDialogDraft draft = SiteSearchDialogDraft.create(null);
        assertFalse(draft.areAllInputsValid());

        draft.setNameValid(true);
        assertFalse(draft.areAllInputsValid());

        draft.setKeywordValid(true);
        assertFalse(draft.areAllInputsValid());

        draft.setUrlValid(true);
        assertTrue(draft.areAllInputsValid());

        draft.setKeywordValid(false);
        assertFalse(draft.areAllInputsValid());
    }
}
