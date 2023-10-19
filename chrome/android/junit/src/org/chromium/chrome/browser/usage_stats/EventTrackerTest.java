// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.usage_stats;

import static org.junit.Assert.assertEquals;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.atLeast;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;
import org.robolectric.annotation.LooperMode;

import org.chromium.base.Callback;
import org.chromium.base.Promise;
import org.chromium.base.test.BaseRobolectricTestRunner;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

/** Unit tests for EventTracker. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@LooperMode(LooperMode.Mode.LEGACY)
public class EventTrackerTest {
    @Mock private UsageStatsBridge mBridge;

    @Captor
    private ArgumentCaptor<
                    Callback<
                            List<
                                    org.chromium.chrome.browser.usage_stats.WebsiteEventProtos
                                            .WebsiteEvent>>>
            mLoadCallbackCaptor;

    @Captor private ArgumentCaptor<Callback<Boolean>> mWriteCallbackCaptor;
    @Captor private ArgumentCaptor<Callback<Boolean>> mDeleteCallbackCaptor;
    @Captor private ArgumentCaptor<String[]> mDeletedDomainsListCaptor;

    private EventTracker mEventTracker;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mEventTracker = new EventTracker(mBridge);
        verify(mBridge, times(1)).getAllEvents(mLoadCallbackCaptor.capture());
    }

    @Test
    public void testRangeQueries() {
        resolveLoadCallback();
        addEntries(100, 2L, 1L, "");
        mEventTracker
                .queryWebsiteEvents(0L, 50L)
                .then(
                        (result) -> {
                            assertEquals(result.size(), 25);
                        });
        mEventTracker
                .queryWebsiteEvents(0L, 49L)
                .then(
                        (result) -> {
                            assertEquals(result.size(), 24);
                        });
        mEventTracker
                .queryWebsiteEvents(0L, 51L)
                .then(
                        (result) -> {
                            assertEquals(result.size(), 25);
                        });
        mEventTracker
                .queryWebsiteEvents(0L, 1000L)
                .then(
                        (result) -> {
                            assertEquals(result.size(), 100);
                        });
        mEventTracker
                .queryWebsiteEvents(1L, 99L)
                .then(
                        (result) -> {
                            assertEquals(result.size(), 49);
                        });
    }

    @Test
    public void testClearAll() {
        resolveLoadCallback();
        addEntries(100, 1L, 0L, "");
        mEventTracker
                .clearAll()
                .then(
                        (dummy) -> {
                            mEventTracker
                                    .queryWebsiteEvents(0L, 1000L)
                                    .then(
                                            (result) -> {
                                                assertEquals(result.size(), 0);
                                            });
                        });

        verify(mBridge, times(1)).deleteAllEvents(mDeleteCallbackCaptor.capture());
        resolveDeleteCallback();
    }

    @Test
    public void testClearRange() {
        resolveLoadCallback();
        addEntries(100, 1L, 0L, "");
        mEventTracker
                .clearRange(0L, 50L)
                .then(
                        (dummy) -> {
                            mEventTracker
                                    .queryWebsiteEvents(0L, 50L)
                                    .then(
                                            (result) -> {
                                                assertEquals(result.size(), 0);
                                            });
                            mEventTracker
                                    .queryWebsiteEvents(50L, 1000L)
                                    .then(
                                            (result) -> {
                                                assertEquals(result.size(), 50);
                                            });
                        });

        verify(mBridge, times(1))
                .deleteEventsInRange(eq(0L), eq(50L), mDeleteCallbackCaptor.capture());
        resolveDeleteCallback();
    }

    @Test
    public void testClearDomains() {
        resolveLoadCallback();
        addEntries(10, 1L, 0L, "a.com");
        addEntries(10, 1L, 10L, "b.com");
        addEntries(10, 1L, 20L, "c.com");
        addEntries(10, 1L, 30L, "b.com");

        List<String> deletedDomains = Arrays.asList("b.com");
        mEventTracker
                .clearDomains(deletedDomains)
                .then(
                        (dummy) -> {
                            mEventTracker
                                    .queryWebsiteEvents(0L, 40L)
                                    .then(
                                            (result) -> {
                                                assertEquals(result.size(), 20);
                                                for (WebsiteEvent event : result.subList(0, 10)) {
                                                    assertEquals(event.getFqdn(), "a.com");
                                                }
                                                for (WebsiteEvent event : result.subList(10, 20)) {
                                                    assertEquals(event.getFqdn(), "c.com");
                                                }
                                            });
                            mEventTracker
                                    .queryWebsiteEvents(0L, 1000L)
                                    .then(
                                            (result) -> {
                                                assertEquals(result.size(), 20);
                                            });
                        });

        verify(mBridge, times(1))
                .deleteEventsWithMatchingDomains(
                        mDeletedDomainsListCaptor.capture(), mDeleteCallbackCaptor.capture());
        resolveDeleteCallback();
        assertEquals(Arrays.asList(mDeletedDomainsListCaptor.getValue()), deletedDomains);
    }

    private void addEntries(int quantity, long stepSize, long startTime, String fqdn) {
        for (int i = 0; i < quantity; i++) {
            Promise<Void> writePromise =
                    mEventTracker.addWebsiteEvent(
                            new WebsiteEvent(startTime, fqdn, WebsiteEvent.EventType.START));
            verify(mBridge, atLeast(1)).addEvents(any(), mWriteCallbackCaptor.capture());
            resolveWriteCallback();
            startTime += stepSize;
        }
    }

    private void resolveLoadCallback() {
        mLoadCallbackCaptor.getValue().onResult(new ArrayList<>());
    }

    private void resolveWriteCallback() {
        mWriteCallbackCaptor.getValue().onResult(true);
    }

    private void resolveDeleteCallback() {
        mDeleteCallbackCaptor.getValue().onResult(true);
    }
}
