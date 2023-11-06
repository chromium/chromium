// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.features.tasks;

import static org.hamcrest.CoreMatchers.equalTo;
import static org.hamcrest.MatcherAssert.assertThat;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import static org.chromium.chrome.features.tasks.TasksSurfaceProperties.FAKE_SEARCH_BOX_CLICK_LISTENER;
import static org.chromium.chrome.features.tasks.TasksSurfaceProperties.FAKE_SEARCH_BOX_TEXT_WATCHER;
import static org.chromium.chrome.features.tasks.TasksSurfaceProperties.INCOGNITO_COOKIE_CONTROLS_MANAGER;
import static org.chromium.chrome.features.tasks.TasksSurfaceProperties.INCOGNITO_LEARN_MORE_CLICK_LISTENER;
import static org.chromium.chrome.features.tasks.TasksSurfaceProperties.IS_FAKE_SEARCH_BOX_VISIBLE;
import static org.chromium.chrome.features.tasks.TasksSurfaceProperties.IS_INCOGNITO_DESCRIPTION_INITIALIZED;
import static org.chromium.chrome.features.tasks.TasksSurfaceProperties.IS_INCOGNITO_DESCRIPTION_VISIBLE;
import static org.chromium.chrome.features.tasks.TasksSurfaceProperties.IS_SURFACE_BODY_VISIBLE;
import static org.chromium.chrome.features.tasks.TasksSurfaceProperties.IS_TAB_CAROUSEL_TITLE_VISIBLE;
import static org.chromium.chrome.features.tasks.TasksSurfaceProperties.IS_TAB_CAROUSEL_VISIBLE;
import static org.chromium.chrome.features.tasks.TasksSurfaceProperties.IS_VOICE_RECOGNITION_BUTTON_VISIBLE;
import static org.chromium.chrome.features.tasks.TasksSurfaceProperties.VOICE_SEARCH_BUTTON_CLICK_LISTENER;

import android.text.Editable;
import android.text.TextWatcher;
import android.view.View;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.feed.FeedReliabilityLogger;
import org.chromium.chrome.browser.ntp.IncognitoCookieControlsManager;
import org.chromium.chrome.browser.omnibox.OmniboxFocusReason;
import org.chromium.chrome.browser.omnibox.OmniboxStub;
import org.chromium.chrome.browser.omnibox.voice.VoiceRecognitionHandler;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.Arrays;
import java.util.List;

/** Tests for {@link TasksSurfaceMediator}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class TasksSurfaceMediatorUnitTest {
    private TasksSurfaceMediator mMediator;

    @Mock private PropertyModel mPropertyModel;
    @Mock private OmniboxStub mOmniboxStub;
    @Mock private VoiceRecognitionHandler mVoiceRecognitionHandler;
    @Mock private View.OnClickListener mLearnMoreOnClickListener;
    @Mock private IncognitoCookieControlsManager mCookieControlsManager;
    @Mock private FeedReliabilityLogger mFeedReliabilityLogger;
    @Captor private ArgumentCaptor<View.OnClickListener> mFakeboxClickListenerCaptor;
    @Captor private ArgumentCaptor<TextWatcher> mFakeboxTextWatcherCaptor;
    @Captor private ArgumentCaptor<View.OnClickListener> mVoiceSearchButtonClickListenerCaptor;
    @Captor private ArgumentCaptor<View.OnClickListener> mLearnMoreOnClickListenerCaptor;
    @Captor private ArgumentCaptor<IncognitoCookieControlsManager> mCookieControlsManagerCaptor;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);

        mMediator =
                new TasksSurfaceMediator(
                        mPropertyModel, mLearnMoreOnClickListener, mCookieControlsManager, true);
        mMediator.initWithNative(mOmniboxStub, mFeedReliabilityLogger);
    }

    @After
    public void tearDown() {
        mMediator = null;
    }

    @Test
    public void initialization() {
        verify(mPropertyModel).set(eq(IS_TAB_CAROUSEL_VISIBLE), eq(true));
        verify(mPropertyModel).set(eq(IS_TAB_CAROUSEL_TITLE_VISIBLE), eq(true));
        verify(mPropertyModel)
                .set(eq(FAKE_SEARCH_BOX_CLICK_LISTENER), mFakeboxClickListenerCaptor.capture());
        verify(mPropertyModel)
                .set(eq(FAKE_SEARCH_BOX_TEXT_WATCHER), mFakeboxTextWatcherCaptor.capture());
        verify(mPropertyModel)
                .set(
                        eq(VOICE_SEARCH_BUTTON_CLICK_LISTENER),
                        mVoiceSearchButtonClickListenerCaptor.capture());
        verify(mPropertyModel).set(eq(IS_FAKE_SEARCH_BOX_VISIBLE), eq(true));
        verify(mPropertyModel).set(eq(IS_VOICE_RECOGNITION_BUTTON_VISIBLE), eq(false));
        verify(mPropertyModel).set(eq(IS_SURFACE_BODY_VISIBLE), eq(true));
        verify(mPropertyModel)
                .set(eq(INCOGNITO_COOKIE_CONTROLS_MANAGER), mCookieControlsManagerCaptor.capture());
        verify(mPropertyModel)
                .set(
                        eq(INCOGNITO_LEARN_MORE_CLICK_LISTENER),
                        mLearnMoreOnClickListenerCaptor.capture());
        assertEquals(mLearnMoreOnClickListener, mLearnMoreOnClickListenerCaptor.getValue());
        assertFalse(mPropertyModel.get(IS_INCOGNITO_DESCRIPTION_VISIBLE));
        assertFalse(mPropertyModel.get(IS_INCOGNITO_DESCRIPTION_INITIALIZED));
    }

    @Test
    public void clickFakebox() {
        verify(mPropertyModel)
                .set(eq(FAKE_SEARCH_BOX_CLICK_LISTENER), mFakeboxClickListenerCaptor.capture());

        mFakeboxClickListenerCaptor.getValue().onClick(null);
        verify(mOmniboxStub, times(1))
                .setUrlBarFocus(
                        eq(true), eq(null), eq(OmniboxFocusReason.TASKS_SURFACE_FAKE_BOX_TAP));
    }

    @Test
    public void longPressFakebox() {
        verify(mPropertyModel)
                .set(eq(FAKE_SEARCH_BOX_TEXT_WATCHER), mFakeboxTextWatcherCaptor.capture());

        String inputText = "test";
        Editable editable = Editable.Factory.getInstance().newEditable(inputText);
        mFakeboxTextWatcherCaptor.getValue().afterTextChanged(editable);
        verify(mOmniboxStub, times(1))
                .setUrlBarFocus(
                        eq(true),
                        eq(inputText),
                        eq(OmniboxFocusReason.TASKS_SURFACE_FAKE_BOX_LONG_PRESS));
        assertThat(editable.length(), equalTo(0));
    }

    @Test
    public void longPressFakeboxWithEmptyText() {
        verify(mPropertyModel)
                .set(eq(FAKE_SEARCH_BOX_TEXT_WATCHER), mFakeboxTextWatcherCaptor.capture());

        // Shouldn't call setUrlBarFocus if the input text is empty.
        Editable editable = Editable.Factory.getInstance().newEditable("");
        mFakeboxTextWatcherCaptor.getValue().afterTextChanged(editable);
        verify(mOmniboxStub, never())
                .setUrlBarFocus(
                        eq(true), eq(""), eq(OmniboxFocusReason.TASKS_SURFACE_FAKE_BOX_LONG_PRESS));
    }

    @Test
    public void clickVoiceRecognitionButton() {
        verify(mPropertyModel)
                .set(
                        eq(VOICE_SEARCH_BUTTON_CLICK_LISTENER),
                        mVoiceSearchButtonClickListenerCaptor.capture());
        doReturn(mVoiceRecognitionHandler).when(mOmniboxStub).getVoiceRecognitionHandler();

        mVoiceSearchButtonClickListenerCaptor.getValue().onClick(null);
        verify(mVoiceRecognitionHandler, times(1))
                .startVoiceRecognition(
                        eq(VoiceRecognitionHandler.VoiceInteractionSource.TASKS_SURFACE));
        verify(mFeedReliabilityLogger, times(1)).onVoiceSearch();
    }

    @Test
    public void performSearchQuery() {
        String queryText = "test";
        List<String> list = Arrays.asList("tbm=nws");
        mMediator.performSearchQuery(queryText, list);
        verify(mOmniboxStub, times(1)).performSearchQuery(eq(queryText), eq(list));
    }
}
