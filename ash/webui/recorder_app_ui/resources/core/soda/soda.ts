// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertExists} from '../utils/assert.js';
import {Infer, z} from '../utils/schema.js';

import {
  FinalResult,
  PartialResult,
  SodaEvent,
  TimeDelta,
  TimingInfo,
} from './types.js';

// A time range in milliseconds.
export const timeRangeSchema = z.object({
  startMs: z.number(),
  endMs: z.number(),
});
export type TimeRange = Infer<typeof timeRangeSchema>;

export const textPartSchema = z.object({
  kind: z.literal('textPart'),
  text: z.string(),
  timeRange: z.nullable(timeRangeSchema),
});

export type TextPart = Infer<typeof textPartSchema>;

export const textSeparatorSchema = z.object({
  kind: z.literal('textSeparator'),
});
export type TextSeparator = Infer<typeof textSeparatorSchema>;
export const textSeparator: TextSeparator = {
  kind: 'textSeparator',
};

export const textTokenSchema = z.union([textPartSchema, textSeparatorSchema]);
export type TextToken = Infer<typeof textTokenSchema>;

function toMs(timeDelta: TimeDelta): number;
function toMs(timeDelta: TimeDelta|null): number|null;
function toMs(timeDelta: TimeDelta|null): number|null {
  if (timeDelta === null) {
    return null;
  }
  return Number(timeDelta.microseconds) / 1e3;
}

function parseTimingInfo(timingInfo: TimingInfo|null): TimeRange|null {
  if (timingInfo === null) {
    return null;
  }
  const {audioStartTime, eventEndTime} = timingInfo;
  return {startMs: toMs(audioStartTime), endMs: toMs(eventEndTime)};
}

function flattenEvent(ev: FinalResult): TextPart[] {
  const {hypothesisPart, timingEvent} = ev;

  const result: TextPart[] = [];
  const eventTimeRange = parseTimingInfo(timingEvent);
  if (eventTimeRange === null) {
    // TODO(pihsun): Check if this can actually happen.
    console.error('soda event has no timestamp', ev);
  }

  if (hypothesisPart === null || hypothesisPart.length === 0) {
    return [];
  }

  for (const [i, part] of hypothesisPart.entries()) {
    const timeRange: TimeRange|null = (() => {
      if (eventTimeRange === null || part.alignment === null) {
        return null;
      }
      const startMs = toMs(part.alignment);
      const endMs = i !== hypothesisPart.length - 1 ?
          toMs(assertExists(hypothesisPart[i + 1]).alignment) :
          eventTimeRange.endMs - eventTimeRange.startMs;
      if (endMs === null) {
        return null;
      }
      // TODO(pihsun): Have a "time" type so we don't have to remember
      // which number is in which unit.
      return {
        startMs: startMs + eventTimeRange.startMs,
        endMs: endMs + eventTimeRange.startMs,
      };
    })();

    result.push({
      kind: 'textPart',
      text: assertExists(part.text[0]),
      timeRange,
    });
  }
  return result;
}

// Transforms the raw soda events into a form that's more easily usable by UI.
export class SodaEventTransformer {
  private readonly tokens: TextToken[] = [];

  // The last PartialResult in SodaEvent with partial result.
  private partialResult: PartialResult|null = null;

  getTokens(): TextToken[] {
    // TODO(pihsun): This happens to always return a new array each time it's
    // called, which triggers lit update. We should have some "computed" values
    // that is only updated when needed.
    const ret = [...this.tokens];
    if (this.partialResult !== null) {
      if (ret.length > 0) {
        ret.push(textSeparator);
      }
      ret.push({
        kind: 'textPart',
        text: assertExists(this.partialResult.partialText[0]),
        timeRange: parseTimingInfo(this.partialResult.timingEvent),
      });
    }
    return ret;
  }

  addEvent(event: SodaEvent): void {
    if ('partialResult' in event) {
      this.partialResult = event.partialResult;
      // Don't update tokens since it'll be added in getTokens.
      return;
    }
    if ('finalResult' in event) {
      // New final result, remove the partial result event.
      this.partialResult = null;
      const {finalResult} = event;
      if (this.tokens.length > 0) {
        this.tokens.push(textSeparator);
      }
      this.tokens.push(...flattenEvent(finalResult));
    } else {
      console.error('unknown event type', event);
    }
  }

  static transformEvents(events: SodaEvent[]): TextToken[] {
    const transformer = new SodaEventTransformer();
    for (const event of events) {
      transformer.addEvent(event);
    }
    return transformer.getTokens();
  }
}

/**
 * Concatenates textTokens into the string representation of the transcription.
 *
 * TODO(pihsun): Have a class for TextToken[] and move this function.
 */
export function concatTextTokens(textTokens: TextToken[]): string {
  const ret: string[] = [];
  // TODO(pihsun): This currently don't include the speaker ID, but since the
  // speaker ID is a little bit not accurate on the start of sentence,
  // including it might make the result weird.
  for (const token of textTokens) {
    if (token.kind === 'textSeparator') {
      continue;
    }
    ret.push(token.text);
  }
  // TODO: b/336919719 - This assumes that tokens are always separated by space,
  // which is not always true (especially for non en-US language). But the
  // "leadingSpace" property that is returned by soda isn't included in the
  // mojo interface for now.
  return ret.join(' ');
}
