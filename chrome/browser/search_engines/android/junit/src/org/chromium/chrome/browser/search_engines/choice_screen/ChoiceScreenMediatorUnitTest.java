// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.search_engines.choice_screen;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import com.google.common.collect.ImmutableList;

import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.mockito.quality.Strictness;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.components.search_engines.FakeTemplateUrl;
import org.chromium.components.search_engines.TemplateUrl;
import org.chromium.ui.modelutil.PropertyModel;

@RunWith(BaseRobolectricTestRunner.class)
@Features.EnableFeatures({ChromeFeatureList.SEARCH_ENGINE_CHOICE})
public class ChoiceScreenMediatorUnitTest {
    public @Rule TestRule mFeatureProcessor = new Features.JUnitProcessor();
    public @Rule MockitoRule mMockitoRule = MockitoJUnit.rule().strictness(Strictness.STRICT_STUBS);

    public @Mock ChoiceScreenDelegate mDelegate;

    private final PropertyModel mModel = ChoiceScreenProperties.createPropertyModel();

    @Test
    public void testNoItems() {
        createMediatorWithItems(ImmutableList.of());

        assertNull(mModel.get(ChoiceScreenProperties.ON_PRIMARY_CLICKED));
        assertEquals(0, mModel.get(ChoiceScreenProperties.ITEM_MODELS).size());
    }

    @Test
    public void testConfirmChoice() {
        PropertyModel itemModel;
        ImmutableList<TemplateUrl> searchEngines =
                ImmutableList.of(
                        new FakeTemplateUrl("Search Engine A", "sea"),
                        new FakeTemplateUrl("Search Engine B", "seb"),
                        new FakeTemplateUrl("Search Engine C", "sec"));
        createMediatorWithItems(searchEngines);

        // Initially, the confirm button is disabled.
        assertNull(mModel.get(ChoiceScreenProperties.ON_PRIMARY_CLICKED));
        assertEquals(3, mModel.get(ChoiceScreenProperties.ITEM_MODELS).size());

        for (int i = 0; i < searchEngines.size(); ++i) {
            TemplateUrl templateUrl = searchEngines.get(i);
            itemModel = mModel.get(ChoiceScreenProperties.ITEM_MODELS).get(i).model;

            assertEquals(
                    templateUrl.getShortName(),
                    itemModel.get(ChoiceScreenProperties.Item.SHORT_NAME));
            // Initially, no item is selected.
            assertFalse(itemModel.get(ChoiceScreenProperties.Item.IS_SELECTED));
            assertNotNull(itemModel.get(ChoiceScreenProperties.Item.ON_CLICKED));
        }

        // Select one item.
        itemModel = mModel.get(ChoiceScreenProperties.ITEM_MODELS).get(1).model;
        assertFalse(itemModel.get(ChoiceScreenProperties.Item.IS_SELECTED));
        itemModel.get(ChoiceScreenProperties.Item.ON_CLICKED).run();

        // It should become selected and the primary action should become enabled.
        assertTrue(itemModel.get(ChoiceScreenProperties.Item.IS_SELECTED));
        assertNotNull(mModel.get(ChoiceScreenProperties.ON_PRIMARY_CLICKED));

        // Confirm the choice.
        mModel.get(ChoiceScreenProperties.ON_PRIMARY_CLICKED).run();

        // Check that the delegate is called and that the confirm button gets disabled.
        verify(mDelegate, times(1)).onChoiceMade(searchEngines.get(1).getKeyword());
        assertNull(mModel.get(ChoiceScreenProperties.ON_PRIMARY_CLICKED));

        // The previously selected item is still selected.
        assertTrue(itemModel.get(ChoiceScreenProperties.Item.IS_SELECTED));

        // Clicking other items does nothing.
        PropertyModel otherItem = mModel.get(ChoiceScreenProperties.ITEM_MODELS).get(2).model;
        otherItem.get(ChoiceScreenProperties.Item.ON_CLICKED).run();
        assertFalse(otherItem.get(ChoiceScreenProperties.Item.IS_SELECTED));
        assertTrue(itemModel.get(ChoiceScreenProperties.Item.IS_SELECTED));
        assertNull(mModel.get(ChoiceScreenProperties.ON_PRIMARY_CLICKED));
    }

    @Test
    public void testChangeSelection() {
        ImmutableList<TemplateUrl> searchEngines =
                ImmutableList.of(
                        new FakeTemplateUrl("Search Engine A", "sea"),
                        new FakeTemplateUrl("Search Engine B", "seb"),
                        new FakeTemplateUrl("Search Engine C", "sec"));
        createMediatorWithItems(searchEngines);

        // Sanity-checking the initial state: 3 items, no selection and the confirm button disabled.
        assertNull(mModel.get(ChoiceScreenProperties.ON_PRIMARY_CLICKED));
        assertEquals(3, mModel.get(ChoiceScreenProperties.ITEM_MODELS).size());
        PropertyModel item0 = mModel.get(ChoiceScreenProperties.ITEM_MODELS).get(0).model;
        PropertyModel item2 = mModel.get(ChoiceScreenProperties.ITEM_MODELS).get(2).model;
        assertFalse(item0.get(ChoiceScreenProperties.Item.IS_SELECTED));
        assertFalse(item2.get(ChoiceScreenProperties.Item.IS_SELECTED));

        // Select item0, it should be selected and the confirm button enabled.
        item0.get(ChoiceScreenProperties.Item.ON_CLICKED).run();

        assertTrue(item0.get(ChoiceScreenProperties.Item.IS_SELECTED));
        assertFalse(item2.get(ChoiceScreenProperties.Item.IS_SELECTED));
        assertNotNull(mModel.get(ChoiceScreenProperties.ON_PRIMARY_CLICKED));

        // Select it again, it should be deselected.
        item0.get(ChoiceScreenProperties.Item.ON_CLICKED).run();

        assertFalse(item0.get(ChoiceScreenProperties.Item.IS_SELECTED));
        assertFalse(item2.get(ChoiceScreenProperties.Item.IS_SELECTED));
        assertNull(mModel.get(ChoiceScreenProperties.ON_PRIMARY_CLICKED));

        // Re-select item 0.
        item0.get(ChoiceScreenProperties.Item.ON_CLICKED).run();

        assertTrue(item0.get(ChoiceScreenProperties.Item.IS_SELECTED));
        assertFalse(item2.get(ChoiceScreenProperties.Item.IS_SELECTED));
        assertNotNull(mModel.get(ChoiceScreenProperties.ON_PRIMARY_CLICKED));

        // Now select item 2.
        item2.get(ChoiceScreenProperties.Item.ON_CLICKED).run();

        assertFalse(item0.get(ChoiceScreenProperties.Item.IS_SELECTED));
        assertTrue(item2.get(ChoiceScreenProperties.Item.IS_SELECTED));
        assertNotNull(mModel.get(ChoiceScreenProperties.ON_PRIMARY_CLICKED));

        // Confirming should notify the delegate with item2's data.
        mModel.get(ChoiceScreenProperties.ON_PRIMARY_CLICKED).run();
        verify(mDelegate, times(1)).onChoiceMade(searchEngines.get(2).getKeyword());
    }

    private void createMediatorWithItems(ImmutableList<TemplateUrl> searchEngines) {
        doReturn(searchEngines).when(mDelegate).getSearchEngines();

        new ChoiceScreenMediator(mModel, mDelegate);
    }
}
