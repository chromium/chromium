// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {Infer, z} from '../utils/schema.js';

// The type is manually constructed from the .mojo at
// chromeos/services/machine_learning/public/mojom/soda.mojom
// TODO(pihsun): Add diarization info when those are implemented.

export const timeDeltaSchema = z.object({
  microseconds: z.bigint(),
});

export type TimeDelta = Infer<typeof timeDeltaSchema>;

export const hypothesisPartSchema = z.object({
  text: z.array(z.string()),
  alignment: z.nullable(timeDeltaSchema),
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
  timingEvent: z.nullable(timingInfoSchema),
});

export type PartialResult = Infer<typeof partialResultSchema>;

export const speechRecognizerEventSchema = z.union([
  z.object({
    finalResult: finalResultSchema,
  }),
  z.object({
    partialResult: partialResultSchema,
  }),
]);

export type SpeechRecognizerEvent = Infer<typeof speechRecognizerEventSchema>;

export const sodaEventSchema = speechRecognizerEventSchema;

export type SodaEvent = Infer<typeof sodaEventSchema>;
