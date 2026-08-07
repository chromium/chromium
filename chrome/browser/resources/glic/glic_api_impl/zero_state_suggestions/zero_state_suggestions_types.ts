// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {ZeroStateSuggestionsOptions, ZeroStateSuggestionsV2} from '../../glic_api/glic_api.js';
import {defInterface, defMessage} from '../transport/messaging.js';
import type {PendingRemote} from '../transport/post_message_transport.js';

// Defines the message exchange between the client suggestions component and
// the host.
export const ZeroStateSuggestionsHostDef = defInterface({
  name: 'ZeroStateSuggestionsHost',
  methods: [
    {
      name: 'getZeroStateSuggestionsAndSubscribe',
      request: defMessage<{
        pendingRemote?: PendingRemote<ZeroStateSuggestionsClient>,
                     options: ZeroStateSuggestionsOptions,
      }>(),
      response: defMessage<{
        suggestions?: ZeroStateSuggestionsV2,
      }>(),
      histogram: {id: 55},
    },
  ],
});
export type ZeroStateSuggestionsHost = typeof ZeroStateSuggestionsHostDef;

export const ZeroStateSuggestionsClientDef = defInterface({
  name: 'ZeroStateSuggestionsClient',
  methods: [
    {
      name: 'zeroStateSuggestionsChanged',
      request: defMessage<{
        suggestions: ZeroStateSuggestionsV2,
        options: ZeroStateSuggestionsOptions,
      }>(),
    },
  ],
});
export type ZeroStateSuggestionsClient = typeof ZeroStateSuggestionsClientDef;
