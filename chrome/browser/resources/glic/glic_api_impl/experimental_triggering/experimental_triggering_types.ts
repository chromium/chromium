// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {Screenshot} from '../../glic_api/glic_api.js';
import {defInterface, defMessage} from '../transport/messaging.js';

export const ExperimentalTriggeringClientDef = defInterface({
  name: 'ExperimentalTriggeringClient',
  methods: [
    {
      name: 'uploadEncryptedScreenshot',
      request: defMessage<{
        screenshot: Screenshot,
      }>(),
      response: defMessage<{
        fileToken: string | null,
      }>(),
    },
    {
      name: 'getExperimentalTriggeringUpdates',
      request: defMessage<{
        observationId: number,
      }>(),
      response: defMessage<{
        success: boolean,
      }>(),
    },
  ],
});

export type ExperimentalTriggeringClient =
    typeof ExperimentalTriggeringClientDef;
