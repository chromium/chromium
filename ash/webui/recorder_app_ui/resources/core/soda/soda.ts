// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertExists} from '../utils/assert.js';
import {Infer, z} from '../utils/schema.js';

import {FinalResult, SodaEvent, TimeDelta, TimingInfo} from './types.js';

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
  leadingSpace: z.nullable(z.boolean()),
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

function parseTimingInfo(
  timingInfo: TimingInfo|null,
  offsetMs: number,
): TimeRange|null {
  if (timingInfo === null) {
    return null;
  }
  const {audioStartTime, eventEndTime} = timingInfo;
  return {
    startMs: toMs(audioStartTime) + offsetMs,
    endMs: toMs(eventEndTime) + offsetMs,
  };
}

function flattenEvent(ev: FinalResult, offsetMs: number): TextPart[] {
  const {hypothesisPart, timingEvent} = ev;

  const result: TextPart[] = [];
  const eventTimeRange = parseTimingInfo(timingEvent, offsetMs);
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
      leadingSpace: part.leadingSpace,
    });
  }
  return result;
}

// Transforms the raw soda events into a form that's more easily usable by UI.
export class SodaEventTransformer {
  private readonly tokens: TextToken[] = [];

  // The last token from the PartialResult in SodaEvent with partial result.
  private partialResultToken: TextToken|null = null;

  getTokens(): TextToken[] {
    // TODO(pihsun): This happens to always return a new array each time it's
    // called, which triggers lit update. We should have some "computed" values
    // that is only updated when needed.
    const ret = [...this.tokens];
    if (this.partialResultToken !== null) {
      if (ret.length > 0) {
        ret.push(textSeparator);
      }
      ret.push(this.partialResultToken);
    }
    return ret;
  }

  /**
   * Adds a SODA event.
   * An offset can be passed to shift the timestamp in the event, since the
   * transcription can be stopped and started while recording.
   *
   * @param event The SODA event.
   * @param offsetMs Offset of the start of the SODA session in microseconds.
   */
  addEvent(event: SodaEvent, offsetMs: number): void {
    if ('partialResult' in event) {
      this.partialResultToken = {
        kind: 'textPart',
        text: assertExists(event.partialResult.partialText[0]),
        timeRange: parseTimingInfo(event.partialResult.timingEvent, offsetMs),
        leadingSpace: true,
      };
      // Don't update tokens since it'll be added in getTokens.
      return;
    }
    if ('finalResult' in event) {
      // New final result, remove the partial result event.
      this.partialResultToken = null;
      const {finalResult} = event;
      if (this.tokens.length > 0) {
        this.tokens.push(textSeparator);
      }
      this.tokens.push(...flattenEvent(finalResult, offsetMs));
    } else {
      console.error('unknown event type', event);
    }
  }
}

/**
 * Concatenates textTokens into the string representation of the transcription.
 *
 * This is also used to export the transcription into a txt file.
 *
 * TODO(pihsun): Have a class for TextToken[] and move this function.
 * TODO(pihsun): Have a different function for exporting to text format and
 * when exporting representation used for summary input.
 * TODO(pihsun): Include speaker ID in the output.
 */
export function concatTextTokens(textTokens: TextToken[]): string {
  const ret: string[] = [];
  let startOfParagraph = true;
  // TODO(pihsun): This currently don't include the speaker ID, but since the
  // speaker ID is a little bit not accurate on the start of sentence,
  // including it might make the result weird.
  for (const token of textTokens) {
    if (token.kind === 'textSeparator') {
      ret.push('\n');
      startOfParagraph = true;
      continue;
    }
    if (!startOfParagraph && (token.leadingSpace ?? true)) {
      ret.push(' ');
    }
    ret.push(token.text);
    startOfParagraph = false;
  }
  return ret.join('');
}
