// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {Observer, Unsubscribe} from '../utils/observer_list.js';
import {Infer, z} from '../utils/schema.js';

// The type is manually constructed from the .mojo at
// chromeos/services/machine_learning/public/mojom/soda.mojom

export const timeDeltaSchema = z.object({
  microseconds: z.bigint(),
});

export type TimeDelta = Infer<typeof timeDeltaSchema>;

export const hypothesisPartSchema = z.object({
  text: z.array(z.string()),
  alignment: z.nullable(timeDeltaSchema),
  leadingSpace: z.nullable(z.boolean()),
  speakerLabel: z.nullable(z.string()),
});

export type HypothesisPart = Infer<typeof hypothesisPartSchema>;

export const timingInfoSchema = z.object({
  audioStartTime: timeDeltaSchema,
  eventEndTime: timeDeltaSchema,
});

export type TimingInfo = Infer<typeof timingInfoSchema>;

export const finalResultSchema = z.object({
  finalHypotheses: z.array(z.string()),
  hypothesisPart: z.nullable(z.array(hypothesisPartSchema)),
  timingEvent: z.nullable(timingInfoSchema),
});

export type FinalResult = Infer<typeof finalResultSchema>;

export const partialResultSchema = z.object({
  partialText: z.array(z.string()),
  hypothesisPart: z.nullable(z.array(hypothesisPartSchema)),
  timingEvent: z.nullable(timingInfoSchema),
});

export type PartialResult = Infer<typeof partialResultSchema>;

export const speakerLabelCorrectionEventSchema = z.object({
  hypothesisParts: z.array(hypothesisPartSchema),
});

export type SpeakerLabelCorrectionEvent =
  Infer<typeof speakerLabelCorrectionEventSchema>;

export const sodaEventSchema = z.union([
  z.object({
    finalResult: finalResultSchema,
  }),
  z.object({
    partialResult: partialResultSchema,
  }),
  z.object({
    labelCorrectionEvent: speakerLabelCorrectionEventSchema,
  }),
]);

export type SodaEvent = Infer<typeof sodaEventSchema>;

export interface SodaSession {
  /**
   * Starts the session.
   *
   * Each session can be started at most once.
   */
  start(): Promise<void>;

  /**
   * Adds audio sample to the session.
   *
   * `start` must be called before this.
   *
   * @param samples Array of sample of the audio. Each sample value is in the
   *     range of [-1, 1].
   */
  addAudio(samples: Float32Array): void;

  /**
   * Stops the session and waits for all audio samples to be processed.
   *
   * This can only be called at most once, and `start` must be called before
   * this.
   */
  stop(): Promise<void>;

  /**
   * Add an observer for the result SodaEvent.
   *
   * Since events before subscriptions are not passed to observer on subscribe,
   * this should be called before audio samples are added to ensure that no
   * event is dropped.
   */
  subscribeEvent(observer: Observer<SodaEvent>): Unsubscribe;
}
