// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {ChangePlaybackSpeedParams, ChangePlaybackVolumeParams, ExportEventParams, FeedbackEventParams, OnboardEventParams, PerfEvent, RecordEventParams, StartSessionEventParams, SuggestTitleEventParams, SummarizeEventParams} from '../../core/events_sender.js';
import {EventsSender as EventsSenderBase} from '../../core/events_sender.js';

export class EventsSender extends EventsSenderBase {
  override sendStartSessionEvent(_: StartSessionEventParams): void {}

  override sendRecordEvent(_: RecordEventParams): void {}

  override sendSuggestTitleEvent(_: SuggestTitleEventParams): void {}

  override sendSummarizeEvent(_: SummarizeEventParams): void {}

  override sendFeedbackTitleSuggestionEvent(_: FeedbackEventParams): void {}

  override sendFeedbackSummaryEvent(_: FeedbackEventParams): void {}

  override sendOnboardEvent(_: OnboardEventParams): void {}

  override sendExportEvent(_: ExportEventParams): void {}

  override sendChangePlaybackSpeedEvent(_: ChangePlaybackSpeedParams): void {}

  override sendChangePlaybackVolumeEvent(_: ChangePlaybackVolumeParams): void {}

  override sendPerfEvent(_event: PerfEvent, _duration: number): void {}
}
