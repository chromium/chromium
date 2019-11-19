// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks;

import static org.hamcrest.CoreMatchers.equalTo;
import static org.hamcrest.MatcherAssert.assertThat;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import static org.chromium.chrome.browser.tasks.TasksSurfaceProperties.FAKE_SEARCH_BOX_CLICK_LISTENER;
import static org.chromium.chrome.browser.tasks.TasksSurfaceProperties.FAKE_SEARCH_BOX_TEXT_WATCHER;
import static org.chromium.chrome.browser.tasks.TasksSurfaceProperties.IS_FAKE_SEARCH_BOX_VISIBLE;
import static org.chromium.chrome.browser.tasks.TasksSurfaceProperties.IS_TAB_CAROUSEL_VISIBLE;
import static org.chromium.chrome.browser.tasks.TasksSurfaceProperties.IS_VOICE_RECOGNITION_BUTTON_VISIBLE;
import static org.chromium.chrome.browser.tasks.TasksSurfaceProperties.VOICE_SEARCH_BUTTON_CLICK_LISTENER;

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

import org.chromium.base.metrics.RecordUserAction;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.ntp.FakeboxDelegate;
import org.chromium.chrome.browser.omnibox.LocationBar;
import org.chromium.chrome.browser.omnibox.LocationBarVoiceRecognitionHandler;
import org.chromium.ui.modelutil.PropertyModel;

/** Tests for {@link TasksSurfaceMediator}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class TasksSurfaceMediatorUnitTest {
    private TasksSurfaceMediator mMediator;

    @Mock
    private PropertyModel mPropertyModel;
    @Mock
    private FakeboxDelegate mFakeboxDelegate;
    @Mock
    private LocationBarVoiceRecognitionHandler mLocationBarVoiceRecognitionHandler;
    @Captor
    private ArgumentCaptor<View.OnClickListener> mFakeboxClickListenerCaptor;
    @Captor
    private ArgumentCaptor<TextWatcher> mFakeboxTextWatcherCaptor;
    @Captor
    private ArgumentCaptor<View.OnClickListener> mVoiceSearchButtonClickListenerCaptor;

    @Before
    public void setUp() {
        RecordUserAction.setDisabledForTests(true);
        MockitoAnnotations.initMocks(this);

        mMediator = new TasksSurfaceMediator(mPropertyModel, mFakeboxDelegate, true);
    }

    @After
    public void tearDown() {
        RecordUserAction.setDisabledForTests(false);
        mMediator = null;
    }

    @Test
    public void initialization() {
        verify(mPropertyModel).set(eq(IS_TAB_CAROUSEL_VISIBLE), eq(true));
        verify(mPropertyModel)
                .set(eq(FAKE_SEARCH_BOX_CLICK_LISTENER), mFakeboxClickListenerCaptor.capture());
        verify(mPropertyModel)
                .set(eq(FAKE_SEARCH_BOX_TEXT_WATCHER), mFakeboxTextWatcherCaptor.capture());
        verify(mPropertyModel)
                .set(eq(VOICE_SEARCH_BUTTON_CLICK_LISTENER),
                        mVoiceSearchButtonClickListenerCaptor.capture());
        verify(mPropertyModel).set(eq(IS_FAKE_SEARCH_BOX_VISIBLE), eq(true));
        verify(mPropertyModel).set(eq(IS_VOICE_RECOGNITION_BUTTON_VISIBLE), eq(false));
    }

    @Test
    public void clickFakebox() {
        verify(mPropertyModel)
                .set(eq(FAKE_SEARCH_BOX_CLICK_LISTENER), mFakeboxClickListenerCaptor.capture());

        mFakeboxClickListenerCaptor.getValue().onClick(null);
        verify(mFakeboxDelegate, times(1))
                .setUrlBarFocus(eq(true), eq(null),
                        eq(LocationBar.OmniboxFocusReason.TASKS_SURFACE_FAKE_BOX_TAP));
    }

    @Test
    public void longPressFakebox() {
        verify(mPropertyModel)
                .set(eq(FAKE_SEARCH_BOX_TEXT_WATCHER), mFakeboxTextWatcherCaptor.capture());

        String inputText = "test";
        Editable editable = Editable.Factory.getInstance().newEditable(inputText);
        mFakeboxTextWatcherCaptor.getValue().afterTextChanged(editable);
        verify(mFakeboxDelegate, times(1))
                .setUrlBarFocus(eq(true), eq(inputText),
                        eq(LocationBar.OmniboxFocusReason.TASKS_SURFACE_FAKE_BOX_LONG_PRESS));
        assertThat(editable.length(), equalTo(0));
    }

    @Test
    public void longPressFakeboxWithEmptyText() {
        verify(mPropertyModel)
                .set(eq(FAKE_SEARCH_BOX_TEXT_WATCHER), mFakeboxTextWatcherCaptor.capture());

        // Shouldn't call setUrlBarFocus if the input text is empty.
        Editable editable = Editable.Factory.getInstance().newEditable("");
        mFakeboxTextWatcherCaptor.getValue().afterTextChanged(editable);
        verify(mFakeboxDelegate, never())
                .setUrlBarFocus(eq(true), eq(""),
                        eq(LocationBar.OmniboxFocusReason.TASKS_SURFACE_FAKE_BOX_LONG_PRESS));
    }

    @Test
    public void clickVoiceRecognitionButton() {
        verify(mPropertyModel)
                .set(eq(VOICE_SEARCH_BUTTON_CLICK_LISTENER),
                        mVoiceSearchButtonClickListenerCaptor.capture());
        doReturn(mLocationBarVoiceRecognitionHandler)
                .when(mFakeboxDelegate)
                .getLocationBarVoiceRecognitionHandler();

        mVoiceSearchButtonClickListenerCaptor.getValue().onClick(null);
        verify(mLocationBarVoiceRecognitionHandler, times(1))
                .startVoiceRecognition(eq(
                        LocationBarVoiceRecognitionHandler.VoiceInteractionSource.TASKS_SURFACE));
    }
}
