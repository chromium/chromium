// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.usage_stats;

import org.chromium.base.CallbackUtils;
import org.chromium.base.Promise;
import org.chromium.chrome.browser.usage_stats.WebsiteEventProtos.Timestamp;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.Iterator;
import java.util.List;
import java.util.concurrent.TimeUnit;
import java.util.function.Function;

/**
 * In-memory store of {@link org.chromium.chrome.browser.usage_stats.WebsiteEvent} objects.
 * Allows for addition of events and querying for all events in a time interval.
 */
public class EventTracker {
    private final UsageStatsBridge mBridge;
    private Promise<List<WebsiteEvent>> mRootPromise;

    public EventTracker(UsageStatsBridge bridge) {
        mBridge = bridge;
        mRootPromise = new Promise<>();
        // We need to add a placeholder exception handler so that Promise doesn't complain when we
        // call variants of then() that don't take a single callback. These variants set an
        // exception handler on the returned promise, so they expect there to be one on the root
        // promise.
        mRootPromise.except(CallbackUtils.emptyCallback());
        mBridge.getAllEvents(
                (result) -> {
                    List<WebsiteEvent> events = new ArrayList<>(result.size());
                    for (WebsiteEventProtos.WebsiteEvent protoEvent : result) {
                        events.add(
                                new WebsiteEvent(
                                        getJavaTimestamp(protoEvent.getTimestamp()),
                                        protoEvent.getFqdn(),
                                        protoEvent.getType().getNumber()));
                    }
                    mRootPromise.fulfill(events);
                });
    }

    /** Query all events in the half-open range [start, end) */
    public Promise<List<WebsiteEvent>> queryWebsiteEvents(long start, long end) {
        assert start < end;
        return mRootPromise.then(
                (Function<List<WebsiteEvent>, List<WebsiteEvent>>)
                        (result) -> {
                            UsageStatsMetricsReporter.reportMetricsEvent(
                                    UsageStatsMetricsEvent.QUERY_EVENTS);
                            List<WebsiteEvent> sublist = sublistFromTimeRange(start, end, result);
                            List<WebsiteEvent> sublistCopy = new ArrayList<>(sublist.size());
                            sublistCopy.addAll(sublist);
                            return sublistCopy;
                        });
    }

    /**
     * Adds an event to the end of the list of events. Adding an event whose timestamp precedes the
     * last event in the list is illegal. The returned promise will be fulfilled once persistence
     * succeeds, and rejected if persistence fails.
     */
    public Promise<Void> addWebsiteEvent(WebsiteEvent event) {
        final Promise<Void> writePromise = new Promise<>();
        mRootPromise.then(
                (result) -> {
                    List<WebsiteEventProtos.WebsiteEvent> eventsList =
                            Arrays.asList(getProtoEvent(event));
                    mBridge.addEvents(
                            eventsList,
                            (didSucceed) -> {
                                if (didSucceed) {
                                    result.add(event);
                                    writePromise.fulfill(null);
                                } else {
                                    writePromise.reject();
                                }
                            });
                },
                CallbackUtils.emptyCallback());

        return writePromise;
    }

    /** Remove every item in the list of events. */
    public Promise<Void> clearAll() {
        final Promise<Void> writePromise = new Promise<>();
        mRootPromise.then(
                (result) -> {
                    mBridge.deleteAllEvents(
                            (didSucceed) -> {
                                if (didSucceed) {
                                    result.clear();
                                    writePromise.fulfill(null);
                                } else {
                                    writePromise.reject();
                                }
                            });
                },
                CallbackUtils.emptyCallback());
        return writePromise;
    }

    /** Removes items in the list in the half-open range [startTimeMs, endTimeMs). */
    public Promise<Void> clearRange(long startTimeMs, long endTimeMs) {
        final Promise<Void> writePromise = new Promise<>();
        mRootPromise.then(
                (result) -> {
                    mBridge.deleteEventsInRange(
                            startTimeMs,
                            endTimeMs,
                            (didSucceed) -> {
                                if (didSucceed) {
                                    sublistFromTimeRange(startTimeMs, endTimeMs, result).clear();
                                    writePromise.fulfill(null);
                                } else {
                                    writePromise.reject();
                                }
                            });
                },
                CallbackUtils.emptyCallback());
        return writePromise;
    }

    /** Clear any events that have a domain in fqdns. */
    public Promise<Void> clearDomains(List<String> fqdns) {
        final Promise<Void> writePromise = new Promise<>();
        mRootPromise.then(
                (result) -> {
                    mBridge.deleteEventsWithMatchingDomains(
                            fqdns.toArray(new String[fqdns.size()]),
                            (didSucceed) -> {
                                if (didSucceed) {
                                    filterMatchingDomains(fqdns, result);
                                    writePromise.fulfill(null);
                                } else {
                                    writePromise.reject();
                                }
                            });
                },
                CallbackUtils.emptyCallback());
        return writePromise;
    }

    private WebsiteEventProtos.WebsiteEvent getProtoEvent(WebsiteEvent event) {
        return WebsiteEventProtos.WebsiteEvent.newBuilder()
                .setFqdn(event.getFqdn())
                .setTimestamp(getProtoTimestamp(event.getTimestamp()))
                .setType(getProtoEventType(event.getType()))
                .build();
    }

    private Timestamp getProtoTimestamp(long timestampMs) {
        return Timestamp.newBuilder()
                .setSeconds(TimeUnit.MILLISECONDS.toSeconds(timestampMs))
                .setNanos((int) TimeUnit.MILLISECONDS.toNanos(timestampMs % 1000))
                .build();
    }

    private WebsiteEventProtos.WebsiteEvent.EventType getProtoEventType(
            @WebsiteEvent.EventType int eventType) {
        switch (eventType) {
            case WebsiteEvent.EventType.START:
                return WebsiteEventProtos.WebsiteEvent.EventType.START_BROWSING;
            case WebsiteEvent.EventType.STOP:
                return WebsiteEventProtos.WebsiteEvent.EventType.STOP_BROWSING;
            default:
                return WebsiteEventProtos.WebsiteEvent.EventType.UNKNOWN;
        }
    }

    private long getJavaTimestamp(Timestamp protoTimestamp) {
        return TimeUnit.SECONDS.toMillis(protoTimestamp.getSeconds())
                + TimeUnit.NANOSECONDS.toMillis(protoTimestamp.getNanos());
    }

    private static List<WebsiteEvent> sublistFromTimeRange(
            long start, long end, List<WebsiteEvent> websiteList) {
        return websiteList.subList(indexOf(start, websiteList), indexOf(end, websiteList));
    }

    private static int indexOf(long time, List<WebsiteEvent> websiteList) {
        for (int i = 0; i < websiteList.size(); i++) {
            if (time <= websiteList.get(i).getTimestamp()) return i;
        }
        return websiteList.size();
    }

    private static void filterMatchingDomains(List<String> fqdns, List<WebsiteEvent> websiteList) {
        Iterator<WebsiteEvent> eventsIterator = websiteList.iterator();
        while (eventsIterator.hasNext()) {
            if (fqdns.contains(eventsIterator.next().getFqdn())) {
                eventsIterator.remove();
            }
        }
    }
}
