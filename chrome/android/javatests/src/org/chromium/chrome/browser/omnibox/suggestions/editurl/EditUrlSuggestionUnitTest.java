// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.editurl;

import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.content.ClipData;
import android.content.ClipboardManager;
import android.view.View;

import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import org.chromium.base.ContextUtils;
import org.chromium.base.UserDataHost;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.UiThreadTest;
import org.chromium.base.test.util.Batch;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.omnibox.OmniboxSuggestionType;
import org.chromium.chrome.browser.omnibox.suggestions.SuggestionHost;
import org.chromium.chrome.browser.omnibox.suggestions.UrlBarDelegate;
import org.chromium.chrome.browser.omnibox.suggestions.base.BaseSuggestionViewProperties;
import org.chromium.chrome.browser.omnibox.suggestions.base.BaseSuggestionViewProperties.Action;
import org.chromium.chrome.browser.omnibox.suggestions.basic.SuggestionViewProperties;
import org.chromium.chrome.browser.search_engines.TemplateUrlServiceFactory;
import org.chromium.chrome.browser.share.ShareDelegate;
import org.chromium.chrome.browser.share.ShareDelegateImpl.ShareOrigin;
import org.chromium.chrome.browser.tab.SadTab;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.chrome.test.util.browser.Features.DisableFeatures;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;
import org.chromium.components.favicon.LargeIconBridge;
import org.chromium.components.omnibox.AutocompleteMatch;
import org.chromium.components.search_engines.TemplateUrlService;
import org.chromium.content_public.browser.test.NativeLibraryTestUtils;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.base.Clipboard;
import org.chromium.ui.base.ClipboardAndroidTestSupport;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.url.GURL;

import java.util.List;

/**
 * Unit tests for the "edit url" omnibox suggestion.
 */
@RunWith(BaseJUnit4ClassRunner.class)
@Batch(Batch.UNIT_TESTS)
public final class EditUrlSuggestionUnitTest {
    private static final String TEST_TITLE = "Test Page";
    private static final String FOOBAR_SEARCH_TERMS = "foobar";
    private static final String BARBAZ_SEARCH_TERMS = "barbaz";

    private static final int ACTION_SHARE = 0;
    private static final int ACTION_COPY = 1;
    private static final int ACTION_EDIT = 2;

    private final GURL mTestUrl = new GURL("http://www.example.com");
    private final GURL mFoobarSearchUrl =
            new GURL("http://www.example.com?q=" + FOOBAR_SEARCH_TERMS);
    private final GURL mBarbazSearchUrl =
            new GURL("http://www.example.com?q=" + BARBAZ_SEARCH_TERMS);
    private EditUrlSuggestionProcessor mProcessor;
    private PropertyModel mModel;

    @Rule
    public TestRule mFeaturesProcessor = new Features.JUnitProcessor();

    @Mock
    private ShareDelegate mShareDelegate;

    @Mock
    private Tab mTab;

    @Mock
    private SadTab mSadTab;

    @Mock
    private AutocompleteMatch mWhatYouTypedSuggestion;

    @Mock
    private AutocompleteMatch mOtherSuggestion;

    @Mock
    private AutocompleteMatch mSearchSuggestion;

    @Mock
    private UrlBarDelegate mUrlBarDelegate;

    @Mock
    private View mEditButton;

    @Mock
    private View mSuggestionView;

    @Mock
    private LargeIconBridge mIconBridge;

    @Mock
    private TemplateUrlService mTemplateUrlService;

    @Mock
    private SuggestionHost mSuggestionHost;

    @Mock
    private ClipboardManager mClipboardManager;

    // The original (real) ClipboardManager to be restored after a test run.
    private ClipboardManager mOldClipboardManager;
    private UserDataHost mUserDataHost;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        NativeLibraryTestUtils.loadNativeLibraryNoBrowserProcess();

        TemplateUrlServiceFactory.setInstanceForTesting(mTemplateUrlService);

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mUserDataHost = new UserDataHost();
            mUserDataHost.setUserData(SadTab.class, mSadTab);

            mOldClipboardManager =
                    Clipboard.getInstance().overrideClipboardManagerForTesting(mClipboardManager);

            mModel = new PropertyModel.Builder(SuggestionViewProperties.ALL_KEYS).build();

            mProcessor = new EditUrlSuggestionProcessor(ContextUtils.getApplicationContext(),
                    mSuggestionHost, mUrlBarDelegate,
                    () -> mIconBridge, () -> mTab, () -> mShareDelegate);
        });

        doReturn(mTestUrl).when(mTab).getUrl();
        doReturn(TEST_TITLE).when(mTab).getTitle();
        doReturn(false).when(mTab).isNativePage();
        doReturn(true).when(mTab).isInitialized();

        // Simulate that all our test tabs are never 'sad'.
        doReturn(mUserDataHost).when(mTab).getUserDataHost();
        doReturn(false).when(mSadTab).isShowing();
        doReturn(OmniboxSuggestionType.URL_WHAT_YOU_TYPED).when(mWhatYouTypedSuggestion).getType();
        doReturn(mTestUrl.getSpec()).when(mWhatYouTypedSuggestion).getDisplayText();
        doReturn(mTestUrl).when(mWhatYouTypedSuggestion).getUrl();

        doReturn(OmniboxSuggestionType.SEARCH_WHAT_YOU_TYPED).when(mSearchSuggestion).getType();
        doReturn(mFoobarSearchUrl).when(mSearchSuggestion).getUrl();
        doReturn(FOOBAR_SEARCH_TERMS).when(mSearchSuggestion).getFillIntoEdit();

        doReturn(OmniboxSuggestionType.SEARCH_HISTORY).when(mOtherSuggestion).getType();
    }

    @After
    public void tearDown() {
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Clipboard.getInstance().overrideClipboardManagerForTesting(mOldClipboardManager);
        });
        ClipboardAndroidTestSupport.cleanup();
    }

    /** Test that the suggestion is triggered. */
    @Test
    @SmallTest
    @UiThreadTest
    public void testUrlSuggestionTriggered() {
        verifyUrlSuggestionTriggered(/* isIncognito */ false);
    }

    /** Test that the suggestion is triggered in Incognito. */
    @Test
    @SmallTest
    @UiThreadTest
    @EnableFeatures(ChromeFeatureList.OMNIBOX_SEARCH_READY_INCOGNITO)
    public void testSuggestionTriggered_Incognito() {
        verifyUrlSuggestionTriggered(/* isIncognito */ true);
    }

    /** Test that the suggestion is not triggered if its url doesn't match the current page's. */
    @Test
    @SmallTest
    @UiThreadTest
    public void testWhatYouTypedWrongUrl() {
        verifyWhatYouTypedWrongUrl(/* isIncognito */ false);
    }

    /**
     * Test that the suggestion is not triggered if its url doesn't match the current page's in
     * Incognito.
     */
    @Test
    @SmallTest
    @UiThreadTest
    @EnableFeatures(ChromeFeatureList.OMNIBOX_SEARCH_READY_INCOGNITO)
    public void testWhatYouTypedWrongUrl_Incognito() {
        verifyWhatYouTypedWrongUrl(/* isIncognito */ true);
    }

    /** Test the edit button is pressed, the correct method in the URL bar delegate is triggered. */
    @Test
    @SmallTest
    @UiThreadTest
    public void testEditButtonPress() {
        verifyEditButtonPress(/* isIncognito */ false);
    }

    /**
     * Test the edit button is pressed, the correct method in the URL bar delegate is triggered in
     * Incognito.
     */
    @Test
    @SmallTest
    @UiThreadTest
    @EnableFeatures(ChromeFeatureList.OMNIBOX_SEARCH_READY_INCOGNITO)
    public void testEditButtonPress_Incognito() {
        verifyEditButtonPress(/* isIncognito */ true);
    }

    /** Test the share button is pressed, we trigger the share menu. */
    @Test
    @SmallTest
    @UiThreadTest
    public void testShareButtonPress() {
        verifyShareButtonPress(/* isIncognito */ false);
    }

    /** Test the share button is pressed, we trigger the share menu in Incognito. */
    @Test
    @SmallTest
    @UiThreadTest
    @EnableFeatures(ChromeFeatureList.OMNIBOX_SEARCH_READY_INCOGNITO)
    public void testShareButtonPress_Incognito() {
        verifyShareButtonPress(/* isIncognito */ true);
    }

    /** Test the copy button is pressed, we update clipboard. */
    @Test
    @SmallTest
    @UiThreadTest
    public void testCopyButtonPress() {
        verifyCopyButtonPress(/* isIncognito */ false);
    }

    /** Test the copy button is pressed, we update clipboard in Incognito. */
    @Test
    @SmallTest
    @UiThreadTest
    @EnableFeatures(ChromeFeatureList.OMNIBOX_SEARCH_READY_INCOGNITO)
    public void testCopyButtonPress_Incognito() {
        verifyCopyButtonPress(/* isIncognito */ true);
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void testSearchSuggestionTriggered() {
        verifySearchSuggestionTriggered(/* isIncognito */ false);
    }

    @Test
    @SmallTest
    @UiThreadTest
    @EnableFeatures(ChromeFeatureList.OMNIBOX_SEARCH_READY_INCOGNITO)
    public void testSearchSuggestionTriggered_Incognito() {
        verifySearchSuggestionTriggered(/* isIncognito */ true);
    }

    @Test
    @SmallTest
    @UiThreadTest
    @DisableFeatures(ChromeFeatureList.OMNIBOX_SEARCH_READY_INCOGNITO)
    public void testSuggestionNotTriggering_IncognitoDisabled() {
        setIncognito(true);

        mProcessor.onUrlFocusChange(true);
        Assert.assertFalse("The processor shouldn't handle the \"what you typed\" suggestion.",
                mProcessor.doesProcessSuggestion(mWhatYouTypedSuggestion, 0));
    }

    public void verifyUrlSuggestionTriggered(boolean isIncognito) {
        setIncognito(isIncognito);
        mProcessor.onUrlFocusChange(true);

        Assert.assertTrue("The processor should handle the \"what you typed\" suggestion.",
                mProcessor.doesProcessSuggestion(mWhatYouTypedSuggestion, 0));

        mProcessor.populateModel(mWhatYouTypedSuggestion, mModel, 0);

        Assert.assertEquals("The model should have the title set.", TEST_TITLE,
                mModel.get(SuggestionViewProperties.TEXT_LINE_1_TEXT).toString());

        Assert.assertEquals("The model should have the URL set to the tab's URL",
                mTestUrl.getSpec(),
                mModel.get(SuggestionViewProperties.TEXT_LINE_2_TEXT).toString());
    }

    public void verifyWhatYouTypedWrongUrl(boolean isIncognito) {
        setIncognito(isIncognito);
        mProcessor.onUrlFocusChange(true);

        when(mWhatYouTypedSuggestion.getUrl()).thenReturn(mFoobarSearchUrl);
        Assert.assertFalse("The processor should not handle the suggestion.",
                mProcessor.doesProcessSuggestion(mWhatYouTypedSuggestion, 0));
    }

    public void verifyEditButtonPress(boolean isIncognito) {
        setIncognito(isIncognito);
        mProcessor.onUrlFocusChange(true);
        mProcessor.doesProcessSuggestion(mWhatYouTypedSuggestion, 0);
        mProcessor.populateModel(mWhatYouTypedSuggestion, mModel, 0);

        List<Action> actions = mModel.get(BaseSuggestionViewProperties.ACTIONS);
        Assert.assertEquals("EditUrl suggestion should have 3 action buttons.", 3, actions.size());
        actions.get(ACTION_EDIT).callback.run();
        verify(mUrlBarDelegate).setOmniboxEditingText(mTestUrl.getSpec());
    }

    private void verifyShareButtonPress(boolean isIncognito) {
        setIncognito(isIncognito);
        mProcessor.onUrlFocusChange(true);
        mProcessor.doesProcessSuggestion(mWhatYouTypedSuggestion, 0);
        mProcessor.populateModel(mWhatYouTypedSuggestion, mModel, 0);

        List<Action> actions = mModel.get(BaseSuggestionViewProperties.ACTIONS);
        Assert.assertEquals("EditUrl suggestion should have 3 action buttons.", 3, actions.size());
        actions.get(ACTION_SHARE).callback.run();
        verify(mShareDelegate, times(1))
                .share(mTab, false /* shareDirectly */, ShareOrigin.EDIT_URL);
    }

    private void verifyCopyButtonPress(boolean isIncognito) {
        setIncognito(isIncognito);
        mProcessor.onUrlFocusChange(true);
        mProcessor.doesProcessSuggestion(mWhatYouTypedSuggestion, 0);
        mProcessor.populateModel(mWhatYouTypedSuggestion, mModel, 0);

        List<Action> actions = mModel.get(BaseSuggestionViewProperties.ACTIONS);

        Assert.assertEquals("EditUrl suggestion should have 3 action buttons.", 3, actions.size());
        ArgumentCaptor<ClipData> argument = ArgumentCaptor.forClass(ClipData.class);
        actions.get(ACTION_COPY).callback.run();
        verify(mClipboardManager, times(1)).setPrimaryClip(argument.capture());

        ClipData clip = ClipData.newPlainText("url", mTestUrl.getSpec());

        // ClipData doesn't implement equals, but their string representations matching should be
        // good enough.
        Assert.assertEquals(clip.toString(), argument.getValue().toString());
    }

    private void verifySearchSuggestionTriggered(boolean isIncognito) {
        setIncognito(isIncognito);
        when(mTab.getUrl()).thenReturn(mFoobarSearchUrl);
        mProcessor.onUrlFocusChange(true);
        when(mTemplateUrlService.getSearchQueryForUrl(mFoobarSearchUrl))
                .thenReturn(FOOBAR_SEARCH_TERMS);
        when(mTemplateUrlService.getSearchQueryForUrl(mBarbazSearchUrl))
                .thenReturn(BARBAZ_SEARCH_TERMS);

        Assert.assertFalse(mProcessor.doesProcessSuggestion(mSearchSuggestion, 0));
        Assert.assertFalse(mProcessor.doesProcessSuggestion(mSearchSuggestion, 1));

        when(mSearchSuggestion.getUrl()).thenReturn(mBarbazSearchUrl);
        when(mSearchSuggestion.getFillIntoEdit()).thenReturn(BARBAZ_SEARCH_TERMS);

        Assert.assertFalse(mProcessor.doesProcessSuggestion(mSearchSuggestion, 0));
        Assert.assertFalse(mProcessor.doesProcessSuggestion(mSearchSuggestion, 1));
    }

    private void setIncognito(boolean isIncognito) {
        when(mTab.isIncognito()).thenReturn(isIncognito);
    }
}
