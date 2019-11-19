// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextualsearch;

import static org.hamcrest.CoreMatchers.is;
import static org.junit.Assert.assertThat;
import static org.mockito.Mockito.any;
import static org.mockito.Mockito.doNothing;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import static org.chromium.chrome.browser.contextualsearch.ContextualSearchTranslationImpl.TranslateBridgeWrapper;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.BlockJUnit4ClassRunner;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import org.chromium.base.test.util.Feature;

import java.util.LinkedHashSet;

/**
 * Tests the {@link ContextualSearchTranslationImpl} class.
 */
@RunWith(BlockJUnit4ClassRunner.class)
public class ContextualSearchTranslationImplTest {
    private static final String ENGLISH = "en";
    private static final String SPANISH = "es";
    private static final String GERMAN = "de";
    private static final LinkedHashSet<String> ENGLISH_AND_SPANISH;
    static {
        LinkedHashSet<String> langs = new LinkedHashSet<String>();
        langs.add(ENGLISH);
        langs.add(SPANISH);
        ENGLISH_AND_SPANISH = langs;
    }
    private static final LinkedHashSet<String> ENGLISH_SET;
    static {
        LinkedHashSet<String> langs = new LinkedHashSet<String>();
        langs.add(ENGLISH);
        ENGLISH_SET = langs;
    }

    @Mock
    private TranslateBridgeWrapper mTranslateBridgeWrapperMock;
    @Mock
    private ContextualSearchRequest mRequest;
    @Mock
    private ContextualSearchPolicy mPolicy;

    private ContextualSearchTranslationImpl mImpl;

    @Before
    public void setup() {
        MockitoAnnotations.initMocks(this);
        mImpl = new ContextualSearchTranslationImpl(mPolicy, mTranslateBridgeWrapperMock);
        doReturn(false).when(mPolicy).isTranslationDisabled();
    }

    @Test
    @Feature("TranslateUtilities")
    public void testNeedsTranslationEmptyModelLanguages() {
        doReturn(new LinkedHashSet<String>()).when(mTranslateBridgeWrapperMock).getModelLanguages();
        assertThat(mImpl.needsTranslation(ENGLISH), is(true));
    }

    @Test
    @Feature("TranslateUtilities")
    public void testNeedsTranslationUserModelLanguages() {
        doReturn(ENGLISH_SET).when(mTranslateBridgeWrapperMock).getModelLanguages();
        assertThat(mImpl.needsTranslation(ENGLISH), is(false));
    }

    @Test
    @Feature("TranslateUtilities")
    public void testNeedsTranslationMultipleModelLanguages() {
        doReturn(ENGLISH_AND_SPANISH).when(mTranslateBridgeWrapperMock).getModelLanguages();
        assertThat(mImpl.needsTranslation(ENGLISH), is(false));
    }

    @Test
    @Feature("TranslateUtilities")
    public void testNeedsTranslationOtherModelLanguage() {
        doReturn(ENGLISH_AND_SPANISH).when(mTranslateBridgeWrapperMock).getModelLanguages();
        assertThat(mImpl.needsTranslation(GERMAN), is(true));
    }

    @Test
    @Feature("TranslateUtilities")
    public void testNeedsTranslationOtherBlocked() {
        doReturn(ENGLISH_AND_SPANISH).when(mTranslateBridgeWrapperMock).getModelLanguages();
        doReturn(true).when(mTranslateBridgeWrapperMock).isBlockedLanguage(GERMAN);
        assertThat(mImpl.needsTranslation(GERMAN), is(false));
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
        doReturn(ENGLISH_AND_SPANISH).when(mTranslateBridgeWrapperMock).getModelLanguages();
        doNothing().when(mRequest).forceTranslation(any(), any());
        when(mRequest.isTranslationForced()).thenReturn(true);

        mImpl.forceTranslateIfNeeded(mRequest, GERMAN);

        assertThat(mRequest.isTranslationForced(), is(true));
        verify(mTranslateBridgeWrapperMock).getModelLanguages();
        verify(mRequest).forceTranslation(GERMAN, null);
    }

    @Test
    @Feature("TranslateUtilities")
    public void testForceTranslateIfNeededWhenNotNeeded() {
        doReturn(ENGLISH_AND_SPANISH).when(mTranslateBridgeWrapperMock).getModelLanguages();

        mImpl.forceTranslateIfNeeded(mRequest, ENGLISH);

        assertThat(mRequest.isTranslationForced(), is(false));
        verify(mTranslateBridgeWrapperMock).getModelLanguages();
        verify(mRequest, never()).forceTranslation(any(), any());
    }
}
