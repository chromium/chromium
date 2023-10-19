// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.suggestions;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.verify;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.thumbnail.generator.ThumbnailProvider;
import org.chromium.chrome.test.util.browser.suggestions.SuggestionsDependenciesRule;
import org.chromium.components.favicon.LargeIconBridge;
import org.chromium.components.favicon.LargeIconBridge.LargeIconCallback;
import org.chromium.url.GURL;
import org.chromium.url.JUnitTestGURLs;

/** Unit tests for {@link ImageFetcher}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class SuggestionsImageFetcherTest {
    public static final int IMAGE_SIZE_PX = 100;
    public static final GURL URL = JUnitTestGURLs.EXAMPLE_URL;

    @Rule public SuggestionsDependenciesRule mSuggestionsDeps = new SuggestionsDependenciesRule();

    @Mock private ThumbnailProvider mThumbnailProvider;
    @Mock private LargeIconBridge mLargeIconBridge;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);

        mSuggestionsDeps.getFactory().largeIconBridge = mLargeIconBridge;
        mSuggestionsDeps.getFactory().thumbnailProvider = mThumbnailProvider;
    }

    @Test
    public void testLargeIconFetch() {
        ImageFetcher imageFetcher = new ImageFetcher(mock(Profile.class));

        imageFetcher.makeLargeIconRequest(URL, IMAGE_SIZE_PX, mock(LargeIconCallback.class));

        verify(mLargeIconBridge)
                .getLargeIconForUrl(eq(URL), eq(IMAGE_SIZE_PX), any(LargeIconCallback.class));
    }
}
