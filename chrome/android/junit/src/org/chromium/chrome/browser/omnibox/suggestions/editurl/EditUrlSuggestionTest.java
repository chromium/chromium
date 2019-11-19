// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.editurl;

import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.content.Context;
import android.content.res.Resources;
import android.support.test.filters.SmallTest;
import android.view.View;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.omnibox.OmniboxSuggestionType;
import org.chromium.chrome.browser.omnibox.suggestions.OmniboxSuggestion;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * Unit tests for the "edit url" omnibox suggestion.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public final class EditUrlSuggestionTest {
    private static final String TEST_URL = "http://www.example.com";
    private static final String TEST_TITLE = "Test Page";

    private EditUrlSuggestionProcessor mProcessor;
    private PropertyModel mModel;

    @Mock
    Context mContext;

    @Mock
    Resources mResources;

    @Mock
    private ActivityTabProvider mTabProvider;

    @Mock
    private Tab mTab;

    @Mock
    private OmniboxSuggestion mWhatYouTypedSuggestion;

    @Mock
    private OmniboxSuggestion mOtherSuggestion;

    @Mock
    private EditUrlSuggestionProcessor.LocationBarDelegate mLocationBarDelegate;

    @Mock
    private EditUrlSuggestionProcessor.SuggestionSelectionHandler mSelectionHandler;

    @Mock
    private View mEditButton;

    @Mock
    private View mSuggestionView;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);

        when(mContext.getResources()).thenReturn(mResources);
        when(mTab.getUrl()).thenReturn(TEST_URL);
        when(mTab.getTitle()).thenReturn(TEST_TITLE);
        when(mTab.isNativePage()).thenReturn(false);
        when(mTab.isIncognito()).thenReturn(false);

        when(mTabProvider.get()).thenReturn(mTab);

        when(mWhatYouTypedSuggestion.getType())
                .thenReturn(OmniboxSuggestionType.URL_WHAT_YOU_TYPED);
        when(mWhatYouTypedSuggestion.getUrl()).thenReturn(TEST_URL);

        when(mOtherSuggestion.getType()).thenReturn(OmniboxSuggestionType.SEARCH_HISTORY);

        mModel = new PropertyModel.Builder(EditUrlSuggestionProperties.ALL_KEYS).build();

        mProcessor = new EditUrlSuggestionProcessor(
                mContext, null, mLocationBarDelegate, mSelectionHandler);
        mProcessor.setActivityTabProvider(mTabProvider);

        when(mEditButton.getId()).thenReturn(R.id.url_edit_icon);
    }

    /** Test that the suggestion is triggered. */
    @Test
    @SmallTest
    public void testSuggestionTriggered() {
        mProcessor.onUrlFocusChange(true);

        Assert.assertTrue("The processor should handle the \"what you typed\" suggestion.",
                mProcessor.doesProcessSuggestion(mWhatYouTypedSuggestion));

        mProcessor.populateModel(mWhatYouTypedSuggestion, mModel, 0);

        Assert.assertEquals("The model should have the title set.", TEST_TITLE,
                mModel.get(EditUrlSuggestionProperties.TITLE_TEXT));

        Assert.assertEquals("The model should have the URL set to the tab's URL", TEST_URL,
                mModel.get(EditUrlSuggestionProperties.URL_TEXT));
    }

    /** Test that the suggestion is not triggered if it is not the first suggestion. */
    @Test
    @SmallTest
    public void testWhatYouTypedSecondSuggestion() {
        mProcessor.onUrlFocusChange(true);

        Assert.assertFalse("The processor should not handle the suggestion.",
                mProcessor.doesProcessSuggestion(mOtherSuggestion));

        Assert.assertFalse("The processor should not handle the \"what you typed\" suggestion.",
                mProcessor.doesProcessSuggestion(mWhatYouTypedSuggestion));
    }

    /** Test the edit button is pressed, the correct method in the URL bar delegate is triggered. */
    @Test
    @SmallTest
    public void testEditButtonPress() {
        mProcessor.onUrlFocusChange(true);
        mProcessor.doesProcessSuggestion(mWhatYouTypedSuggestion);
        mProcessor.populateModel(mWhatYouTypedSuggestion, mModel, 0);

        mModel.get(EditUrlSuggestionProperties.BUTTON_CLICK_LISTENER).onClick(mEditButton);

        verify(mLocationBarDelegate).setOmniboxEditingText(TEST_URL);
    }

    /** Test that when suggestion is tapped, it still navigates to the correct location. */
    @Test
    @SmallTest
    public void testPressSuggestion() {
        mProcessor.onUrlFocusChange(true);
        mProcessor.doesProcessSuggestion(mWhatYouTypedSuggestion);
        mProcessor.populateModel(mWhatYouTypedSuggestion, mModel, 0);

        mModel.get(EditUrlSuggestionProperties.BUTTON_CLICK_LISTENER).onClick(mSuggestionView);

        verify(mSelectionHandler).onEditUrlSuggestionSelected(mWhatYouTypedSuggestion);
    }
}
