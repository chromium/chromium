// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.dom_distiller;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.reset;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.shared_preferences.SharedPreferencesManager;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.components.dom_distiller.core.DomDistillerFeatures;

import java.util.concurrent.TimeUnit;

/** Unit tests for {@link ReaderModeActionRateLimiter}. */
@RunWith(BaseRobolectricTestRunner.class)
@EnableFeatures(DomDistillerFeatures.READER_MODE_DISTILL_IN_APP)
public class ReaderModeActionRateLimiterTest {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    private SharedPreferencesManager mPrefs;
    private ReaderModeActionRateLimiter mReaderModeActionRateLimiter;

    private @Mock ReaderModeActionRateLimiter.Observer mObserver;

    @Before
    public void setUp() {
        mPrefs = ChromeSharedPreferences.getInstance();
        mReaderModeActionRateLimiter = ReaderModeActionRateLimiter.getInstance();
        mReaderModeActionRateLimiter.addObserver(mObserver);
    }

    @After
    public void tearDown() {
        mReaderModeActionRateLimiter.removeObserver(mObserver);
        mPrefs.removeKey(ChromePreferenceKeys.READER_MODE_ACTION_SHOW_COUNT);
        mPrefs.removeKey(ChromePreferenceKeys.READER_MODE_ACTION_FIRST_SHOWN_TIMESTAMP);
        mPrefs.removeKey(ChromePreferenceKeys.READER_MODE_ACTION_SUPPRESSION_END_TIMESTAMP);
    }

    @Test
    public void testIsActionSuppressed_default() {
        assertFalse(mReaderModeActionRateLimiter.isActionSuppressed());
    }

    @Test
    public void testIsActionSuppressed_suppressed() {
        long suppressionEnd = System.currentTimeMillis() + TimeUnit.DAYS.toMillis(1);
        mPrefs.writeLong(
                ChromePreferenceKeys.READER_MODE_ACTION_SUPPRESSION_END_TIMESTAMP, suppressionEnd);
        assertTrue(mReaderModeActionRateLimiter.isActionSuppressed());
    }

    @Test
    public void testIsActionSuppressed_suppressionExpired() {
        long suppressionEnd = System.currentTimeMillis() - TimeUnit.DAYS.toMillis(1);
        mPrefs.writeLong(
                ChromePreferenceKeys.READER_MODE_ACTION_SUPPRESSION_END_TIMESTAMP, suppressionEnd);
        assertFalse(mReaderModeActionRateLimiter.isActionSuppressed());
    }

    @Test
    public void testOnActionShown_incrementsCount() {
        mReaderModeActionRateLimiter.onActionShown();
        assertEquals(1, mPrefs.readInt(ChromePreferenceKeys.READER_MODE_ACTION_SHOW_COUNT));
    }

    @Test
    public void testOnActionShown_setsFirstTimestamp() {
        mReaderModeActionRateLimiter.onActionShown();
        assertTrue(
                mPrefs.readLong(ChromePreferenceKeys.READER_MODE_ACTION_FIRST_SHOWN_TIMESTAMP) > 0);
    }

    @Test
    public void testOnActionShown_resetsAfterWindow() {
        long firstShown = System.currentTimeMillis() - TimeUnit.DAYS.toMillis(2);
        mPrefs.writeLong(ChromePreferenceKeys.READER_MODE_ACTION_FIRST_SHOWN_TIMESTAMP, firstShown);
        mPrefs.writeInt(ChromePreferenceKeys.READER_MODE_ACTION_SHOW_COUNT, 2);

        mReaderModeActionRateLimiter.onActionShown();

        assertEquals(1, mPrefs.readInt(ChromePreferenceKeys.READER_MODE_ACTION_SHOW_COUNT));
        assertTrue(
                mPrefs.readLong(ChromePreferenceKeys.READER_MODE_ACTION_FIRST_SHOWN_TIMESTAMP)
                        > firstShown);
    }

    @Test
    public void testOnActionShown_suppressesAtLimit() {
        createTemporarySuppression();
    }

    @Test
    public void testOnActionShown_permanentSuppressionAfterMultipleSuppressions() {
        for (int i = 0; i < 3; i++) {
            createTemporarySuppression();
            resetTemporarySuppression();
        }

        assertTrue(mReaderModeActionRateLimiter.isActionSuppressed());
    }

    @Test
    public void testOnActionClicked_clearsPrefs() {
        mPrefs.writeInt(ChromePreferenceKeys.READER_MODE_ACTION_SHOW_COUNT, 2);
        mPrefs.writeLong(ChromePreferenceKeys.READER_MODE_ACTION_FIRST_SHOWN_TIMESTAMP, 12345L);
        mPrefs.writeLong(ChromePreferenceKeys.READER_MODE_ACTION_SUPPRESSION_END_TIMESTAMP, 56789L);

        mReaderModeActionRateLimiter.onActionClicked();

        assertFalse(mPrefs.contains(ChromePreferenceKeys.READER_MODE_ACTION_SHOW_COUNT));
        assertFalse(mPrefs.contains(ChromePreferenceKeys.READER_MODE_ACTION_FIRST_SHOWN_TIMESTAMP));
        assertFalse(
                mPrefs.contains(ChromePreferenceKeys.READER_MODE_ACTION_SUPPRESSION_END_TIMESTAMP));
    }

    private void createTemporarySuppression() {
        reset(mObserver);
        for (int i = 0; i < 3; i++) {
            mReaderModeActionRateLimiter.onActionShown();
        }
        assertTrue(mReaderModeActionRateLimiter.isActionSuppressed());
        assertTrue(
                mPrefs.readLong(ChromePreferenceKeys.READER_MODE_ACTION_SUPPRESSION_END_TIMESTAMP)
                        > 0);
    }

    private void resetTemporarySuppression() {
        mPrefs.removeKeySync(ChromePreferenceKeys.READER_MODE_ACTION_SUPPRESSION_END_TIMESTAMP);
        mPrefs.removeKeySync(ChromePreferenceKeys.READER_MODE_ACTION_SHOW_COUNT);
    }
}
