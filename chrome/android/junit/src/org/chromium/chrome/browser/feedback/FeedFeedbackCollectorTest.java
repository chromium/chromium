// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feedback;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.os.Bundle;
import android.os.Looper;

import androidx.annotation.Nullable;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;
import org.robolectric.annotation.LooperMode;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.Callback;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.signin.identitymanager.IdentityManager;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

/** Test for {@link FeedFeedbackCollector}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@LooperMode(LooperMode.Mode.LEGACY)
public class FeedFeedbackCollectorTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();
    // Enable the Features class, so we can override command line switches in the test.
    @Mock private Activity mActivity;
    @Mock private Profile mProfile;
    @Mock private CoreAccountInfo mAccountInfo;

    // Test constants.
    private static final String CATEGORY_TAG = "category_tag";
    private static final String DESCRIPTION = "description";

    public static final String CARD_URL = "CardUrl";
    public static final String CARD_PUBLISHER = "CardPublisher";
    public static final String CARD_PUBLISHING_DATE = "CardPublishingDate";
    public static final String CARD_TITLE = "CardTitle";

    public static final String THE_URL = "https://www.google.com";
    public static final String THE_PUBLISHER = "Google";
    public static final String THE_PUBLISHING_DATE = "July 4, 1776";
    public static final String THE_TITLE = "Declaration of Independence";

    private static void verifySynchronousSources(Bundle bundle) {
        assertTrue(bundle.containsKey(CARD_URL));
        assertTrue(bundle.containsKey(CARD_PUBLISHER));
        assertTrue(bundle.containsKey(CARD_PUBLISHING_DATE));
        assertTrue(bundle.containsKey(CARD_TITLE));
        assertEquals(THE_URL, bundle.getString(CARD_URL));
        assertEquals(THE_PUBLISHER, bundle.getString(CARD_PUBLISHER));
        assertEquals(THE_PUBLISHING_DATE, bundle.getString(CARD_PUBLISHING_DATE));
        assertEquals(THE_TITLE, bundle.getString(CARD_TITLE));
    }

    private static class EmptyFeedFeedbackCollector extends FeedFeedbackCollector {
        EmptyFeedFeedbackCollector(
                Activity activity,
                Profile profile,
                @Nullable String url,
                @Nullable String categoryTag,
                @Nullable String description,
                @Nullable ScreenshotSource screenshotSource,
                @Nullable Map<String, String> feedContext,
                Callback<FeedbackCollector> callback) {
            super(
                    activity,
                    categoryTag,
                    description,
                    screenshotSource,
                    new FeedFeedbackCollector.InitParams(profile, url, feedContext),
                    callback,
                    null);
        }

        // Override the async feedback sources to return an empty list, so we are only testing ths
        // sync sources.
        @Override
        protected List<AsyncFeedbackSource> buildAsynchronousFeedbackSources(
                FeedFeedbackCollector.InitParams initParams) {
            return new ArrayList<>();
        }
    }

    @Before
    public void setUp() {
        ThreadUtils.setUiThread(Looper.getMainLooper());
        when(mAccountInfo.getEmail()).thenReturn(null);
        IdentityServicesProvider.setInstanceForTests(mock(IdentityServicesProvider.class));
        when(IdentityServicesProvider.get().getIdentityManager(any()))
                .thenReturn(mock(IdentityManager.class));
        when(IdentityServicesProvider.get()
                        .getIdentityManager(any())
                        .getPrimaryAccountInfo(anyInt()))
                .thenReturn(mAccountInfo);
    }

    @Test
    @Feature({"Feed"})
    public void testFeedSynchronousData() {
        @SuppressWarnings("unchecked")
        Callback<FeedbackCollector> callback = mock(Callback.class);
        Map<String, String> feedContext = new HashMap<String, String>();
        feedContext.put(CARD_URL, THE_URL);
        feedContext.put(CARD_PUBLISHER, THE_PUBLISHER);
        feedContext.put(CARD_PUBLISHING_DATE, THE_PUBLISHING_DATE);
        feedContext.put(CARD_TITLE, THE_TITLE);

        FeedFeedbackCollector collector =
                new EmptyFeedFeedbackCollector(
                        mActivity,
                        mProfile,
                        null,
                        CATEGORY_TAG,
                        DESCRIPTION,
                        null,
                        feedContext,
                        (result) -> callback.onResult(result));

        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        verify(callback, times(1)).onResult(collector);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    verifySynchronousSources(collector.getBundle());
                    assertFalse(
                            collector
                                    .getBundle()
                                    .containsKey(
                                            FeedbackContextFeedbackSource.FEEDBACK_CONTEXT_KEY));
                    assertEquals(CATEGORY_TAG, collector.getCategoryTag());
                    assertEquals(DESCRIPTION, collector.getDescription());
                });
    }
}
