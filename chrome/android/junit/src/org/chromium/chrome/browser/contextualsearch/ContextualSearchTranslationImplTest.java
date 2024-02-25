// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextualsearch;

import static androidx.test.espresso.matcher.ViewMatchers.assertThat;

import static org.hamcrest.CoreMatchers.is;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.doNothing;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.contextualsearch.ContextualSearchTranslationImpl.TranslateBridgeWrapper;

import java.util.ArrayList;

/** Tests the {@link ContextualSearchTranslationImpl} class. */
@RunWith(BaseRobolectricTestRunner.class)
public class ContextualSearchTranslationImplTest {
    private static final String ENGLISH = "en";
    private static final String SPANISH = "es";
    private static final String GERMAN = "de";
    private static final ArrayList<String> ENGLISH_AND_SPANISH;

    static {
        ArrayList<String> langs = new ArrayList<String>();
        langs.add(ENGLISH);
        langs.add(SPANISH);
        ENGLISH_AND_SPANISH = langs;
    }

    private static final ArrayList<String> ENGLISH_LIST;

    static {
        ArrayList<String> langs = new ArrayList<String>();
        langs.add(ENGLISH);
        ENGLISH_LIST = langs;
    }

    @Mock private TranslateBridgeWrapper mTranslateBridgeWrapperMock;
    @Mock private ContextualSearchRequest mRequest;
    @Mock private ContextualSearchPolicy mPolicy;

    private ContextualSearchTranslationImpl mImpl;

    @Before
    public void setup() {
        MockitoAnnotations.initMocks(this);
        mImpl = new ContextualSearchTranslationImpl(mTranslateBridgeWrapperMock);
    }

    @Test
    @Feature("TranslateUtilities")
    public void testNeedsTranslationEmptyFluentLanguages() {
        doReturn(new ArrayList<String>())
                .when(mTranslateBridgeWrapperMock)
                .getNeverTranslateLanguages();
        assertThat(mImpl.needsTranslation(ENGLISH), is(true));
    }

    @Test
    @Feature("TranslateUtilities")
    public void testNeedsTranslationOneFluentLanguage() {
        doReturn(ENGLISH_LIST).when(mTranslateBridgeWrapperMock).getNeverTranslateLanguages();
        assertThat(mImpl.needsTranslation(ENGLISH), is(false));
    }

    @Test
    @Feature("TranslateUtilities")
    public void testNeedsTranslationMultipleFluentLanguages() {
        doReturn(ENGLISH_AND_SPANISH)
                .when(mTranslateBridgeWrapperMock)
                .getNeverTranslateLanguages();
        assertThat(mImpl.needsTranslation(ENGLISH), is(false));
    }

    @Test
    @Feature("TranslateUtilities")
    public void testNeedsTranslationOtherFluentLanguage() {
        doReturn(ENGLISH_AND_SPANISH)
                .when(mTranslateBridgeWrapperMock)
                .getNeverTranslateLanguages();
        assertThat(mImpl.needsTranslation(GERMAN), is(true));
    }

    @Test
    @Feature("TranslateUtilities")
    public void testGetFluentLanguages() {
        doReturn(ENGLISH_AND_SPANISH)
                .when(mTranslateBridgeWrapperMock)
                .getNeverTranslateLanguages();
        assertThat(mImpl.getTranslateServiceFluentLanguages(), is(ENGLISH + "," + SPANISH));
    }

    @Test
    @Feature("TranslateUtilities")
    public void testTargetLanguage() {
        doReturn(GERMAN).when(mTranslateBridgeWrapperMock).getTargetLanguage();
        assertThat(mImpl.getTranslateServiceTargetLanguage(), is(GERMAN));
    }

    @Test
    @Feature("TranslateUtilities")
    public void testForceTranslateIfNeededWhenNeeded() {
        doReturn(ENGLISH_AND_SPANISH)
                .when(mTranslateBridgeWrapperMock)
                .getNeverTranslateLanguages();
        doNothing().when(mRequest).forceTranslation(any(), any());
        when(mRequest.isTranslationForced()).thenReturn(true);

        mImpl.forceTranslateIfNeeded(mRequest, GERMAN, true);

        assertThat(mRequest.isTranslationForced(), is(true));
        verify(mTranslateBridgeWrapperMock).getNeverTranslateLanguages();
        verify(mRequest).forceTranslation(GERMAN, null);
    }

    @Test
    @Feature("TranslateUtilities")
    public void testForceTranslateIfNeededWhenNotNeeded() {
        doReturn(ENGLISH_AND_SPANISH)
                .when(mTranslateBridgeWrapperMock)
                .getNeverTranslateLanguages();

        mImpl.forceTranslateIfNeeded(mRequest, ENGLISH, true);

        assertThat(mRequest.isTranslationForced(), is(false));
        verify(mTranslateBridgeWrapperMock).getNeverTranslateLanguages();
        verify(mRequest, never()).forceTranslation(any(), any());
    }
}
