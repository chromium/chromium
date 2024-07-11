// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feedback;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoMoreInteractions;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.graphics.Bitmap;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.text.TextUtils;
import android.util.Pair;

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
import org.chromium.base.CollectionUtil;
import org.chromium.base.ThreadUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.signin.identitymanager.IdentityManager;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import java.util.Map;

/** Test for {@link ChromeFeedbackCollector}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@LooperMode(LooperMode.Mode.LEGACY)
public class ChromeFeedbackCollectorUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Mock private Activity mActivity;
    @Mock private Profile mProfile;
    @Mock private CoreAccountInfo mAccountInfo;

    // Test constants.
    private static final String CATEGORY_TAG = "category_tag";
    private static final String DESCRIPTION = "description";
    private static final String FEEDBACK_CONTEXT = "feedback_context";
    private static final String ACCOUNT_IN_USE = "foo@gmail.com";
    private static final String KEY_1 = "key1";
    private static final String KEY_2 = "key2";
    private static final String KEY_3 = "key3";
    private static final String KEY_4 = "key4";
    private static final String KEY_5 = "key5";
    private static final String KEY_6 = "key6";
    private static final String KEY_7 = "key7";
    private static final String KEY_8 = "key8";
    private static final String KEY_9 = "key9";
    private static final String KEY_10 = "key10";

    private static final String VALUE_1 = "value1";
    private static final String VALUE_2 = "value2";
    private static final String VALUE_3 = "value3";
    private static final String VALUE_4 = "value4";
    private static final String VALUE_5 = "value5";
    private static final String VALUE_6 = "value6";
    private static final String VALUE_7 = "value7";
    private static final String VALUE_8 = "value8";
    private static final String VALUE_9 = "value9";
    private static final String VALUE_10 = "value10";

    private static List<FeedbackSource> buildSynchronousFeedbackSources() {
        Map<String, String> map1 =
                CollectionUtil.newHashMap(Pair.create(KEY_1, VALUE_1), Pair.create(KEY_2, VALUE_2));
        Map<String, String> map2 = CollectionUtil.newHashMap(Pair.create(KEY_3, VALUE_3));

        Pair<String, String> logs1 = Pair.create(KEY_4, VALUE_4);
        Pair<String, String> logs2 = Pair.create(KEY_5, VALUE_5);

        return Arrays.asList(
                new MockFeedbackSource(map1, null),
                new MockFeedbackSource(map2, logs1),
                new MockFeedbackSource(null, logs2),
                new MockFeedbackSource(null, null));
    }

    private static void verifySynchronousSources(Bundle bundle, Map<String, String> logs) {
        assertTrue(bundle.containsKey(KEY_1));
        assertTrue(bundle.containsKey(KEY_1));
        assertTrue(bundle.containsKey(KEY_3));
        assertEquals(VALUE_1, bundle.getString(KEY_1));
        assertEquals(VALUE_2, bundle.getString(KEY_2));
        assertEquals(VALUE_3, bundle.getString(KEY_3));

        assertTrue(logs.containsKey(KEY_4));
        assertTrue(logs.containsKey(KEY_5));
        assertEquals(VALUE_4, logs.get(KEY_4));
        assertEquals(VALUE_5, logs.get(KEY_5));
    }

    private static List<AsyncFeedbackSource> buildAsyncronousFeedbackSources() {
        Map<String, String> map1 =
                CollectionUtil.newHashMap(Pair.create(KEY_6, VALUE_6), Pair.create(KEY_7, VALUE_7));
        Map<String, String> map2 = CollectionUtil.newHashMap(Pair.create(KEY_8, VALUE_8));

        Pair<String, String> logs1 = Pair.create(KEY_9, VALUE_9);
        Pair<String, String> logs2 = Pair.create(KEY_10, VALUE_10);

        return Arrays.asList(
                new MockAsyncFeedbackSource(map1, null),
                new MockAsyncFeedbackSource(map2, logs1),
                new MockAsyncFeedbackSource(null, logs2),
                new MockAsyncFeedbackSource(null, null));
    }

    private static void verifyAsynchronousSources(Bundle bundle, Map<String, String> logs) {
        assertTrue(bundle.containsKey(KEY_6));
        assertTrue(bundle.containsKey(KEY_7));
        assertTrue(bundle.containsKey(KEY_8));
        assertEquals(VALUE_6, bundle.getString(KEY_6));
        assertEquals(VALUE_7, bundle.getString(KEY_7));
        assertEquals(VALUE_8, bundle.getString(KEY_8));

        assertTrue(logs.containsKey(KEY_9));
        assertTrue(logs.containsKey(KEY_10));
        assertEquals(VALUE_9, logs.get(KEY_9));
        assertEquals(VALUE_10, logs.get(KEY_10));
    }

    // Helper classes to make mocking and validating the work correct.
    private static class MockScreenshotSource implements ScreenshotSource {
        private Runnable mCallback;

        private boolean mDone;
        private Bitmap mBitmap;

        // ScreenshotSource implementation.
        @Override
        public void capture(Runnable callback) {
            mCallback = callback;
        }

        @Override
        public Bitmap getScreenshot() {
            return mBitmap;
        }

        @Override
        public boolean isReady() {
            return mDone;
        }

        public void triggerDone(Bitmap bitmap) {
            assertNotEquals(null, mCallback);

            mDone = true;
            mBitmap = bitmap;
            new Handler(Looper.getMainLooper()).post(mCallback);
        }
    }

    private static class MockFeedbackSource implements FeedbackSource {
        private final Map<String, String> mFeedback;
        private final Pair<String, String> mLogs;

        MockFeedbackSource(Map<String, String> feedback, Pair<String, String> logs) {
            mFeedback = feedback;
            mLogs = logs;
        }

        @Override
        public Map<String, String> getFeedback() {
            return mFeedback;
        }

        @Override
        public Pair<String, String> getLogs() {
            return mLogs;
        }
    }

    private static class MockAsyncFeedbackSource implements AsyncFeedbackSource {
        private Runnable mCallback;
        private boolean mDone;
        private Map<String, String> mFeedback;
        private Pair<String, String> mLogs;

        MockAsyncFeedbackSource(Map<String, String> feedback, Pair<String, String> logs) {
            mFeedback = feedback;
            mLogs = logs;
        }

        // AsyncFeedbackSource implementation.
        @Override
        public void start(Runnable callback) {
            mCallback = callback;
        }

        @Override
        public boolean isReady() {
            return mDone;
        }

        @Override
        public Map<String, String> getFeedback() {
            return mFeedback;
        }

        @Override
        public Pair<String, String> getLogs() {
            return mLogs;
        }

        public void triggerDone() {
            assertNotEquals(null, mCallback);

            mDone = true;
            new Handler(Looper.getMainLooper()).post(mCallback);
        }
    }

    private static class EmptyChromeFeedbackCollector extends ChromeFeedbackCollector {
        EmptyChromeFeedbackCollector(
                Activity activity,
                Profile profile,
                @Nullable String url,
                @Nullable String categoryTag,
                @Nullable String description,
                @Nullable String feedbackContext,
                @Nullable ScreenshotSource screenshotSource,
                Callback<FeedbackCollector> callback) {
            super(
                    activity,
                    categoryTag,
                    description,
                    screenshotSource,
                    new ChromeFeedbackCollector.InitParams(profile, url, feedbackContext),
                    callback,
                    null);
        }

        // ChromeFeedbackCollector implementation.
        @Override
        protected List<FeedbackSource> buildSynchronousFeedbackSources(
                Activity activity, ChromeFeedbackCollector.InitParams initParams) {
            return new ArrayList<>();
        }

        @Override
        protected List<AsyncFeedbackSource> buildAsynchronousFeedbackSources(
                ChromeFeedbackCollector.InitParams initParams) {
            return new ArrayList<>();
        }
    }

    private static Bitmap createBitmap() {
        return Bitmap.createBitmap(1, 1, Bitmap.Config.ARGB_8888);
    }

    @Before
    public void setUp() {
        when(mAccountInfo.getEmail()).thenReturn(ACCOUNT_IN_USE);
        IdentityServicesProvider.setInstanceForTests(mock(IdentityServicesProvider.class));
        when(IdentityServicesProvider.get().getIdentityManager(any()))
                .thenReturn(mock(IdentityManager.class));
        when(IdentityServicesProvider.get()
                        .getIdentityManager(any())
                        .getPrimaryAccountInfo(anyInt()))
                .thenReturn(mAccountInfo);
    }

    @Test
    @Feature({"Feedback"})
    public void testRecordLatencyHistogram() {
        @SuppressWarnings("unchecked")
        Callback<FeedbackCollector> callback = mock(Callback.class);

        ChromeFeedbackCollector collector =
                new EmptyChromeFeedbackCollector(
                        mActivity,
                        mProfile,
                        null,
                        null,
                        null,
                        null,
                        null,
                        (result) -> callback.onResult(result));

        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        verify(callback, times(1)).onResult(any());

        assertEquals(
                1,
                RecordHistogram.getHistogramTotalCountForTesting(
                        "Feedback.Duration.FetchSystemInformation"));
    }

    @Test
    @Feature({"Feedback"})
    public void testNoMetaData() {
        @SuppressWarnings("unchecked")
        Callback<FeedbackCollector> callback = mock(Callback.class);

        ChromeFeedbackCollector collector =
                new EmptyChromeFeedbackCollector(
                        mActivity,
                        mProfile,
                        null,
                        null,
                        null,
                        null,
                        null,
                        (result) -> callback.onResult(result));

        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        verify(callback, times(1)).onResult(any());

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    assertTrue(TextUtils.isEmpty(collector.getCategoryTag()));
                    assertTrue(TextUtils.isEmpty(collector.getDescription()));
                    assertTrue(collector.getBundle().isEmpty());
                    assertTrue(collector.getLogs().isEmpty());
                    assertNull(collector.getScreenshot());
                });
    }

    @Test
    @Feature({"Feedback"})
    public void testBasicSynchronousData() {
        @SuppressWarnings("unchecked")
        Callback<FeedbackCollector> callback = mock(Callback.class);

        ChromeFeedbackCollector collector =
                new EmptyChromeFeedbackCollector(
                        mActivity,
                        mProfile,
                        null,
                        CATEGORY_TAG,
                        DESCRIPTION,
                        null,
                        null,
                        (result) -> callback.onResult(result)) {
                    @Override
                    protected List<FeedbackSource> buildSynchronousFeedbackSources(
                            Activity activity, ChromeFeedbackCollector.InitParams initParams) {
                        return ChromeFeedbackCollectorUnitTest.buildSynchronousFeedbackSources();
                    }
                };

        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        verify(callback, times(1)).onResult(collector);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    verifySynchronousSources(collector.getBundle(), collector.getLogs());
                    assertFalse(
                            collector
                                    .getBundle()
                                    .containsKey(
                                            FeedbackContextFeedbackSource.FEEDBACK_CONTEXT_KEY));
                    assertEquals(CATEGORY_TAG, collector.getCategoryTag());
                    assertEquals(DESCRIPTION, collector.getDescription());
                    assertNull(collector.getScreenshot());
                    assertEquals(ACCOUNT_IN_USE, collector.getAccountInUse());
                });
    }

    @Test
    @Feature({"Feedback"})
    public void testNullIdentityService() {
        IdentityServicesProvider.setInstanceForTests(mock(IdentityServicesProvider.class));
        when(IdentityServicesProvider.get().getIdentityManager(any())).thenReturn(null);

        @SuppressWarnings("unchecked")
        Callback<FeedbackCollector> callback = mock(Callback.class);

        ChromeFeedbackCollector collector =
                new EmptyChromeFeedbackCollector(
                        mActivity,
                        mProfile,
                        null,
                        CATEGORY_TAG,
                        DESCRIPTION,
                        null,
                        null,
                        (result) -> callback.onResult(result)) {
                    @Override
                    protected List<FeedbackSource> buildSynchronousFeedbackSources(
                            Activity activity, ChromeFeedbackCollector.InitParams initParams) {
                        return ChromeFeedbackCollectorUnitTest.buildSynchronousFeedbackSources();
                    }
                };

        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        verify(callback, times(1)).onResult(collector);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    assertEquals(null, collector.getAccountInUse());
                });
    }

    @Test
    @Feature({"Feedback"})
    public void testBasicSynchronousDataWithFeedbackContext() {
        @SuppressWarnings("unchecked")
        Callback<FeedbackCollector> callback = mock(Callback.class);

        ChromeFeedbackCollector collector =
                new EmptyChromeFeedbackCollector(
                        mActivity,
                        mProfile,
                        null,
                        CATEGORY_TAG,
                        DESCRIPTION,
                        FEEDBACK_CONTEXT,
                        null,
                        (result) -> callback.onResult(result)) {
                    @Override
                    protected List<FeedbackSource> buildSynchronousFeedbackSources(
                            Activity activity, ChromeFeedbackCollector.InitParams initParams) {
                        ArrayList<FeedbackSource> list =
                                new ArrayList<>(
                                        ChromeFeedbackCollectorUnitTest
                                                .buildSynchronousFeedbackSources());
                        list.add(new FeedbackContextFeedbackSource(FEEDBACK_CONTEXT));
                        return list;
                    }
                };

        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        verify(callback, times(1)).onResult(collector);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    verifySynchronousSources(collector.getBundle(), collector.getLogs());
                    assertTrue(
                            collector
                                    .getBundle()
                                    .containsKey(
                                            FeedbackContextFeedbackSource.FEEDBACK_CONTEXT_KEY));
                    assertEquals(
                            FEEDBACK_CONTEXT,
                            collector
                                    .getBundle()
                                    .get(FeedbackContextFeedbackSource.FEEDBACK_CONTEXT_KEY));
                    assertEquals(CATEGORY_TAG, collector.getCategoryTag());
                    assertEquals(DESCRIPTION, collector.getDescription());
                    assertNull(collector.getScreenshot());
                });
    }

    @Test
    @Feature({"Feedback"})
    public void testBasicAsynchronousData() {
        @SuppressWarnings("unchecked")
        Callback<FeedbackCollector> callback = mock(Callback.class);

        final List<AsyncFeedbackSource> sources = buildAsyncronousFeedbackSources();

        ChromeFeedbackCollector collector =
                new EmptyChromeFeedbackCollector(
                        mActivity,
                        mProfile,
                        null,
                        CATEGORY_TAG,
                        DESCRIPTION,
                        null,
                        null,
                        (result) -> callback.onResult(result)) {
                    @Override
                    protected List<AsyncFeedbackSource> buildAsynchronousFeedbackSources(
                            ChromeFeedbackCollector.InitParams initParams) {
                        return sources;
                    }
                };

        sources.forEach(source -> ((MockAsyncFeedbackSource) source).triggerDone());
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        verify(callback, times(1)).onResult(collector);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    verifyAsynchronousSources(collector.getBundle(), collector.getLogs());
                    assertEquals(CATEGORY_TAG, collector.getCategoryTag());
                    assertEquals(DESCRIPTION, collector.getDescription());
                    assertNull(collector.getScreenshot());
                });
    }

    @Test
    @Feature({"Feedback"})
    public void testBasicMixedData() {
        @SuppressWarnings("unchecked")
        Callback<FeedbackCollector> callback = mock(Callback.class);

        final List<AsyncFeedbackSource> sources = buildAsyncronousFeedbackSources();

        ChromeFeedbackCollector collector =
                new EmptyChromeFeedbackCollector(
                        mActivity,
                        mProfile,
                        null,
                        CATEGORY_TAG,
                        DESCRIPTION,
                        null,
                        null,
                        (result) -> callback.onResult(result)) {
                    @Override
                    protected List<AsyncFeedbackSource> buildAsynchronousFeedbackSources(
                            ChromeFeedbackCollector.InitParams initParams) {
                        return sources;
                    }

                    @Override
                    protected List<FeedbackSource> buildSynchronousFeedbackSources(
                            Activity activity, ChromeFeedbackCollector.InitParams initParams) {
                        return ChromeFeedbackCollectorUnitTest.buildSynchronousFeedbackSources();
                    }
                };

        sources.forEach(source -> ((MockAsyncFeedbackSource) source).triggerDone());
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        verify(callback, times(1)).onResult(collector);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Bundle bundle = collector.getBundle();
                    Map<String, String> logs = collector.getLogs();
                    verifySynchronousSources(bundle, logs);
                    verifyAsynchronousSources(bundle, logs);
                    assertEquals(CATEGORY_TAG, collector.getCategoryTag());
                    assertEquals(DESCRIPTION, collector.getDescription());
                    assertNull(collector.getScreenshot());
                });
    }

    @Test
    @Feature({"Feedback"})
    public void testAsynchronousDataTimeout() {
        @SuppressWarnings("unchecked")
        Callback<FeedbackCollector> callback = mock(Callback.class);

        final List<AsyncFeedbackSource> sources = buildAsyncronousFeedbackSources();

        ChromeFeedbackCollector collector =
                new EmptyChromeFeedbackCollector(
                        mActivity,
                        mProfile,
                        null,
                        CATEGORY_TAG,
                        DESCRIPTION,
                        null,
                        null,
                        (result) -> callback.onResult(result)) {
                    @Override
                    protected List<AsyncFeedbackSource> buildAsynchronousFeedbackSources(
                            ChromeFeedbackCollector.InitParams initParams) {
                        return sources;
                    }
                };

        // Do not trigger done.  The collector should respond back anyway and still try to build the
        // logs and feedback report.
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        verify(callback, times(1)).onResult(collector);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    verifyAsynchronousSources(collector.getBundle(), collector.getLogs());
                    assertEquals(CATEGORY_TAG, collector.getCategoryTag());
                    assertEquals(DESCRIPTION, collector.getDescription());
                    assertNull(collector.getScreenshot());
                });
    }

    @Test
    @Feature({"Feedback"})
    public void testScreenshot() {
        @SuppressWarnings("unchecked")
        Callback<FeedbackCollector> callback = mock(Callback.class);

        final List<AsyncFeedbackSource> sources = buildAsyncronousFeedbackSources();

        MockScreenshotSource mockScreenshotSource = new MockScreenshotSource();
        EmptyChromeFeedbackCollector collector =
                new EmptyChromeFeedbackCollector(
                        mActivity,
                        mProfile,
                        null,
                        CATEGORY_TAG,
                        DESCRIPTION,
                        null,
                        mockScreenshotSource,
                        (result) -> callback.onResult(result));

        Bitmap bitmap = createBitmap();
        mockScreenshotSource.triggerDone(bitmap);

        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        verify(callback, times(1)).onResult(collector);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    assertEquals(CATEGORY_TAG, collector.getCategoryTag());
                    assertEquals(DESCRIPTION, collector.getDescription());
                    assertEquals(bitmap, collector.getScreenshot());
                    assertTrue(collector.getBundle().isEmpty());
                    assertTrue(collector.getLogs().isEmpty());
                });
    }

    @Test
    @Feature({"Feedback"})
    public void testScreenshotBypassesTimeout() {
        @SuppressWarnings("unchecked")
        Callback<FeedbackCollector> callback = mock(Callback.class);

        final List<AsyncFeedbackSource> sources = buildAsyncronousFeedbackSources();

        MockScreenshotSource mockScreenshotSource = new MockScreenshotSource();
        EmptyChromeFeedbackCollector collector =
                new EmptyChromeFeedbackCollector(
                        mActivity,
                        mProfile,
                        null,
                        CATEGORY_TAG,
                        DESCRIPTION,
                        null,
                        mockScreenshotSource,
                        (result) -> callback.onResult(result));

        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        // We should not get a callback until the screenshot task finishes, even if that extends
        // beyond our internal timeouts.
        verify(callback, times(0)).onResult(collector);

        Bitmap bitmap = createBitmap();
        mockScreenshotSource.triggerDone(bitmap);

        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        verify(callback, times(1)).onResult(collector);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    assertEquals(CATEGORY_TAG, collector.getCategoryTag());
                    assertEquals(DESCRIPTION, collector.getDescription());
                    assertEquals(bitmap, collector.getScreenshot());
                    assertTrue(collector.getBundle().isEmpty());
                    assertTrue(collector.getLogs().isEmpty());
                });
    }

    @Test
    @Feature({"Feedback"})
    public void testNullScreenshotOverrideStillTriggersCallback() {
        @SuppressWarnings("unchecked")
        Callback<FeedbackCollector> callback = mock(Callback.class);

        final List<AsyncFeedbackSource> sources = buildAsyncronousFeedbackSources();

        EmptyChromeFeedbackCollector collector =
                new EmptyChromeFeedbackCollector(
                        mActivity,
                        mProfile,
                        null,
                        CATEGORY_TAG,
                        DESCRIPTION,
                        null,
                        new MockScreenshotSource(),
                        (result) -> callback.onResult(result));

        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();

        // We should not get a callback until the screenshot task finishes, even if that extends
        // beyond our internal timeouts.
        verify(callback, times(0)).onResult(collector);

        ThreadUtils.runOnUiThreadBlocking(() -> assertNull(collector.getScreenshot()));
        ThreadUtils.runOnUiThreadBlocking(() -> collector.setScreenshot(null));

        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        verify(callback, times(1)).onResult(collector);

        ThreadUtils.runOnUiThreadBlocking(() -> assertNull(collector.getScreenshot()));
    }

    @Test
    @Feature({"Feedback"})
    public void testScreenshotOverrideStillTriggersCallback() {
        @SuppressWarnings("unchecked")
        Callback<FeedbackCollector> callback = mock(Callback.class);

        final List<AsyncFeedbackSource> sources = buildAsyncronousFeedbackSources();

        MockScreenshotSource mockScreenshotSource = new MockScreenshotSource();
        EmptyChromeFeedbackCollector collector =
                new EmptyChromeFeedbackCollector(
                        mActivity,
                        mProfile,
                        null,
                        CATEGORY_TAG,
                        DESCRIPTION,
                        null,
                        mockScreenshotSource,
                        (result) -> callback.onResult(result));

        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();

        // We should not get a callback until the screenshot task finishes, even if that extends
        // beyond our internal timeouts.
        verify(callback, times(0)).onResult(collector);
        ThreadUtils.runOnUiThreadBlocking(() -> assertNull(collector.getScreenshot()));

        Bitmap bitmap = createBitmap();
        ThreadUtils.runOnUiThreadBlocking(() -> collector.setScreenshot(bitmap));

        mockScreenshotSource.triggerDone(null);
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        verify(callback, times(1)).onResult(collector);

        ThreadUtils.runOnUiThreadBlocking(() -> assertEquals(bitmap, collector.getScreenshot()));
    }

    @Test
    @Feature({"Feedback"})
    public void testScreenshotOverrideWithNoOriginalScreenshot() {
        @SuppressWarnings("unchecked")
        Callback<FeedbackCollector> callback = mock(Callback.class);

        final List<AsyncFeedbackSource> sources = buildAsyncronousFeedbackSources();

        EmptyChromeFeedbackCollector collector =
                new EmptyChromeFeedbackCollector(
                        mActivity,
                        mProfile,
                        null,
                        CATEGORY_TAG,
                        DESCRIPTION,
                        null,
                        null,
                        (result) -> callback.onResult(result));

        Bitmap bitmap = createBitmap();
        ThreadUtils.runOnUiThreadBlocking(() -> collector.setScreenshot(bitmap));

        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();

        verify(callback, times(1)).onResult(collector);
        ThreadUtils.runOnUiThreadBlocking(() -> assertEquals(bitmap, collector.getScreenshot()));
    }

    @Test
    @Feature({"Feedback"})
    public void testScreenshotOverrideAfterCallback() {
        @SuppressWarnings("unchecked")
        Callback<FeedbackCollector> callback = mock(Callback.class);

        final List<AsyncFeedbackSource> sources = buildAsyncronousFeedbackSources();

        MockScreenshotSource mockScreenshotSource = new MockScreenshotSource();
        EmptyChromeFeedbackCollector collector =
                new EmptyChromeFeedbackCollector(
                        mActivity,
                        mProfile,
                        null,
                        CATEGORY_TAG,
                        DESCRIPTION,
                        null,
                        mockScreenshotSource,
                        (result) -> callback.onResult(result));

        {
            mockScreenshotSource.triggerDone(null);
            ShadowLooper.runUiThreadTasksIncludingDelayedTasks();

            verify(callback, times(1)).onResult(collector);
        }

        Bitmap bitmap = createBitmap();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    collector.setScreenshot(bitmap);

                    // Check that immediately after setting the screenshot it is available.
                    assertEquals(bitmap, collector.getScreenshot());
                });

        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        ThreadUtils.runOnUiThreadBlocking(() -> assertEquals(bitmap, collector.getScreenshot()));

        // If we have already gotten a callback, we should not get another one.
        verifyNoMoreInteractions(callback);
    }

    @Test
    @Feature({"Feedback"})
    public void testOldScreenshotDoesNotOverrideNewOne() {
        @SuppressWarnings("unchecked")
        Callback<FeedbackCollector> callback = mock(Callback.class);

        final List<AsyncFeedbackSource> sources = buildAsyncronousFeedbackSources();

        MockScreenshotSource mockScreenshotSource = new MockScreenshotSource();

        EmptyChromeFeedbackCollector collector =
                new EmptyChromeFeedbackCollector(
                        mActivity,
                        mProfile,
                        null,
                        CATEGORY_TAG,
                        DESCRIPTION,
                        null,
                        mockScreenshotSource,
                        (result) -> callback.onResult(result));

        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();

        // We should not get a callback until the screenshot task finishes, even if that extends
        // beyond our internal timeouts.
        verify(callback, times(0)).onResult(collector);
        ThreadUtils.runOnUiThreadBlocking(() -> assertNull(collector.getScreenshot()));

        Bitmap bitmap = createBitmap();
        ThreadUtils.runOnUiThreadBlocking(() -> collector.setScreenshot(bitmap));

        Bitmap bitmap2 = createBitmap();
        mockScreenshotSource.triggerDone(bitmap2);
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();
        verify(callback, times(1)).onResult(collector);

        ThreadUtils.runOnUiThreadBlocking(() -> assertEquals(bitmap, collector.getScreenshot()));
    }
}
