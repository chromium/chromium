// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {
  EventsSender as EventsSenderBase,
  RecordEventParams,
  StartSessionEventParams,
  SuggestTitleEventParams,
  SummarizeEventParams,
} from '../../core/events_sender.js';

export class EventsSender extends EventsSenderBase {
  override sendStartSessionEvent(_: StartSessionEventParams): void {}

  override sendRecordEvent(_: RecordEventParams): void {}

  override sendSuggestTitleEvent(_: SuggestTitleEventParams): void {}

  override sendSummarizeEvent(_: SummarizeEventParams): void {}
}
