// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {EventsSender, PerfEvent} from './events_sender.js';

type EventType = PerfEvent['kind'];

interface PerfEventValue {
  event: PerfEvent;
  startTime: number;
}

export class PerfLogger {
  private readonly perfEventMap = new Map<EventType, PerfEventValue>();

  constructor(private readonly eventsSender: EventsSender) {}

  start(event: PerfEvent): void {
    // TODO: b/327538356 - Use assert when verified that there is no error.
    if (this.perfEventMap.has(event.kind)) {
      console.error(`Perf event ${event.kind} already exists.`);
    }
    this.perfEventMap.set(event.kind, {
      event,
      startTime: performance.now(),
    });
  }

  /**
   * Finishes the event if it has started.
   *
   * This avoids `finish` error when we only record `eventType` initiated from
   * particular sources, e.g. UI.
   */
  tryFinish(eventType: EventType): void {
    if (!this.perfEventMap.has(eventType)) {
      return;
    }
    this.finish(eventType);
  }

  /**
   * Terminates and sends the perf event.
   */
  finish(eventType: EventType): void {
    const eventValue = this.perfEventMap.get(eventType);
    if (eventValue === undefined) {
      console.error(`Perf event ${eventType} has not started logging.`);
      return;
    }

    const {event, startTime} = eventValue;
    const duration = Math.round(performance.now() - startTime);

    this.eventsSender.sendPerfEvent(event, duration);
    this.perfEventMap.delete(eventType);
  }
}
