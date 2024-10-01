// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {i18n} from '../i18n.js';
import {assertExists} from '../utils/assert.js';
import {Infer, z} from '../utils/schema.js';
import {lazyInit, sliceWhen} from '../utils/utils.js';

import {
  FinalResult,
  PartialResult,
  SodaEvent,
  SpeakerLabelCorrectionEvent,
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
  leadingSpace: z.nullable(z.boolean()),
  speakerLabel: z.autoNullOptional(z.string()),
  // Since the transcription saved to the disk are always finalResult, and this
  // is only used in intermediate partialResult, only include this field in
  // partialResult to save some disk space.
  partial: z.optional(z.literal(true)),
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

function flattenEvent(
  ev: FinalResult|PartialResult,
  offsetMs: number,
  speakerLabelEnabled: boolean,
  isPartialResult = false,
): TextPart[] {
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
      speakerLabel: speakerLabelEnabled ? part.speakerLabel : null,
      partial: isPartialResult ? true : undefined,
    });
  }
  return result;
}

// Transforms the raw soda events into a form that's more easily usable by UI.
export class SodaEventTransformer {
  private readonly tokens: TextToken[] = [];

  // The last tokens from the PartialResult in SodaEvent with partial result.
  private partialResultTokens: TextToken[]|null = null;

  constructor(private readonly speakerLabelEnabled: boolean) {}

  getTranscription(shouldFinalizeTranscription = false): Transcription {
    const tokens = [...this.tokens];
    if (this.partialResultTokens !== null) {
      if (tokens.length > 0) {
        tokens.push(textSeparator);
      }
      if (shouldFinalizeTranscription) {
        const partialResultTokens = structuredClone(this.partialResultTokens);
        for (const token of partialResultTokens) {
          if (token.kind === 'textPart') {
            delete token.partial;
          }
        }
        tokens.push(...partialResultTokens);
      } else {
        tokens.push(...this.partialResultTokens);
      }
    }
    return new Transcription(tokens);
  }

  private handleSpeakerLabelCorrectionEvent(
    ev: SpeakerLabelCorrectionEvent,
    offsetMs: number,
  ) {
    if (!this.speakerLabelEnabled) {
      // Don't handle speaker label correction event when it's not enabled.
      return;
    }
    const {hypothesisParts} = ev;
    for (const correctionPart of hypothesisParts) {
      const speakerLabel = correctionPart.speakerLabel ?? null;
      const startMs = toMs(correctionPart.alignment);
      if (startMs === null) {
        console.error('speaker label correction event without timestamp', ev);
        continue;
      }
      // We search backward since it's more likely that the corrected token is
      // recent.
      // TODO(pihsun): assert that the tokens have increasing timestamp, and
      // binary search for efficiency.
      let found = false;
      for (let i = this.tokens.length - 1; i >= 0; i--) {
        const token = assertExists(this.tokens[i]);
        if (token.kind === 'textSeparator') {
          continue;
        }
        if (token.timeRange?.startMs === startMs + offsetMs &&
            token.text === correctionPart.text[0]) {
          // TODO(pihsun): This inline updates this.tokens, which works now
          // since getTokens always return a copy, but ideally we want either
          // immutable update, or signal/proxy with nested change detection, or
          // have a clearer boundary on which values (especially object/array)
          // should be immutably updated for lit change detection.
          token.speakerLabel = speakerLabel;
          found = true;
          break;
        }
      }
      if (!found) {
        console.error(
          'speaker label correction event without corresponding previous part?',
          ev,
        );
      }
    }
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
      this.partialResultTokens = flattenEvent(
        event.partialResult,
        offsetMs,
        this.speakerLabelEnabled,
        /* isPartialResult= */ true,
      );
      // Don't update tokens since it'll be added in getTokens.
      return;
    }
    if ('finalResult' in event) {
      // New final result, remove the partial result event.
      this.partialResultTokens = null;
      const {finalResult} = event;
      if (this.tokens.length > 0) {
        this.tokens.push(textSeparator);
      }
      this.tokens.push(
        ...flattenEvent(finalResult, offsetMs, this.speakerLabelEnabled),
      );
      return;
    }
    if ('labelCorrectionEvent' in event) {
      this.handleSpeakerLabelCorrectionEvent(
        event.labelCorrectionEvent,
        offsetMs,
      );
      return;
    }
    console.error('unknown event type', event);
  }
}

export const transcriptionSchema = z.transform(
  z.object({
    // Transcriptions in form of text tokens.
    //
    // Since transcription can be enabled / disabled during the recording, the
    // `textTokens` might only contain part of the transcription when
    // transcription is enabled.
    //
    // If the transcription is never enabled while recording, `textTokens` will
    // be null (to show a different state in playback view).
    textTokens: z.nullable(z.array(textTokenSchema)),
  }),
  {
    test(input) {
      return input instanceof Transcription;
    },
    decode({textTokens}) {
      if (textTokens === null) {
        return null;
      }
      return new Transcription(textTokens);
    },
    encode(val) {
      if (val === null) {
        return {textTokens: null};
      }
      return {textTokens: val.textTokens};
    },
  },
);

const MAX_DESCRIPTION_LENGTH = 512;

export class Transcription {
  constructor(readonly textTokens: TextToken[]) {}

  isEmpty(): boolean {
    return this.textTokens.length === 0;
  }

  get wordCount(): number {
    // TODO(kamchonlathorn): The definition of "word count" can be ambiguous and
    // the word count for non-English languages can be different.
    return this.textTokens.filter((token) => token.kind === 'textPart').length;
  }

  /**
   * Concatenates textTokens into the string representation of the
   * transcription.
   *
   * This is used for title generation and summary input.
   */
  toPlainText = lazyInit((): string => {
    const ret: string[] = [];
    let startOfParagraph = true;
    // TODO(pihsun): This currently don't include the speaker label, but since
    // the speaker label is a little bit not accurate on the start of sentence,
    // including it might make the result weird.
    for (const token of this.textTokens) {
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
  });

  /**
   * Concatenates textTokens into the string representation of the
   * transcription. For continuous paragraphs with the same speaker label, adds
   * the speaker label at the first paragraph.
   *
   * This is used to export the transcription into a txt file.
   */
  toExportText = lazyInit((): string => {
    const ret: string[] = [];
    let startOfParagraph = true;
    let currentSpeaker: string|null = null;
    for (const token of this.textTokens) {
      if (token.kind === 'textSeparator') {
        ret.push('\n');
        startOfParagraph = true;
        continue;
      }
      if (token.speakerLabel !== currentSpeaker) {
        if (!startOfParagraph) {
          ret.push('\n');
          startOfParagraph = true;
        }
        if (ret.length !== 0) {
          // Add a new line between two speakers.
          ret.push('\n');
        }
        if (token.speakerLabel !== null) {
          ret.push(i18n.transcriptionSpeakerLabelLabel(token.speakerLabel));
          ret.push('\n');
        }
        currentSpeaker = token.speakerLabel;
      }
      if (!startOfParagraph && (token.leadingSpace ?? true)) {
        ret.push(' ');
      }
      ret.push(token.text);
      startOfParagraph = false;
    }
    return ret.join('');
  });

  toShortDescription = lazyInit((): string => {
    if (this.textTokens === null) {
      return '';
    }
    const transcription = this.toPlainText();
    if (transcription.length <= MAX_DESCRIPTION_LENGTH - 3) {
      return transcription;
    }
    return transcription.substring(0, MAX_DESCRIPTION_LENGTH - 3) + '...';
  });

  /**
   * Gets the list of speaker label in the transcription.
   *
   * The returned label is ordered by the first appearance of the label in the
   * transcription.
   */
  getSpeakerLabels = lazyInit((): string[] => {
    const speakerLabels = new Set<string>();
    for (const token of this.textTokens) {
      if (token.kind === 'textPart' && token.speakerLabel !== null &&
          !token.partial) {
        speakerLabels.add(token.speakerLabel);
      }
    }
    return Array.from(speakerLabels);
  });

  /**
   * Splits the transcription into several paragraphs.
   *
   * Each paragraph have continuous timestamp, and a single speaker label.
   */
  getParagraphs = lazyInit((): TextPart[][] => {
    const slicedTokens = sliceWhen(this.textTokens, (a, b) => {
      if (a.kind === 'textSeparator' || b.kind === 'textSeparator') {
        return true;
      }
      if (a.timeRange === null && b.timeRange === null) {
        return false;
      }
      if (a.timeRange?.endMs !== b.timeRange?.startMs) {
        // TODO(pihsun): This currently is not used since we already
        // split across result border, and within the same result the
        // time ranges are always continuous.
        return true;
      }
      if (a.partial !== b.partial) {
        return true;
      }
      if (!a.partial && a.speakerLabel !== b.speakerLabel) {
        return true;
      }
      return false;
    });

    return slicedTokens.filter((tokens) => {
      return tokens.every((t) => t.kind === 'textPart');
    });
  });
}
