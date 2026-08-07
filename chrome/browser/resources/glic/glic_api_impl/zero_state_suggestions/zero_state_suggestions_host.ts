// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {ZeroStateSuggestionsClientReceiver} from '../../glic.mojom-webui.js';
import type {ZeroStateSuggestionsClientInterface, ZeroStateSuggestionsHandlerInterface, ZeroStateSuggestionsOptions as ZeroStateSuggestionsOptionsMojo, ZeroStateSuggestionsV2 as ZeroStateSuggestionsV2Mojo} from '../../glic.mojom-webui.js';
import type {ZeroStateSuggestionsOptions, ZeroStateSuggestionsV2} from '../../glic_api/glic_api.js';
import {zeroStateSuggestionsToClient} from '../host/conversions.js';
import {createForwardingMojoRemote} from '../host/host_utils.js';
import type {MessageHandlerInterface} from '../transport/messaging.js';
import type {PendingRemote, PostMessageRemote, PostMessageRouter} from '../transport/post_message_transport.js';

import type {ZeroStateSuggestionsClient, ZeroStateSuggestionsHost} from './zero_state_suggestions_types.js';

export class ZeroStateSuggestionsHostMessageHandler implements
    MessageHandlerInterface<ZeroStateSuggestionsHost> {
  private clientReceiver: ZeroStateSuggestionsClientReceiver|null = null;

  constructor(
      private handler: ZeroStateSuggestionsHandlerInterface,
      private router: PostMessageRouter,
  ) {}

  getZeroStateSuggestionsAndSubscribe(payload: {
    pendingRemote?: PendingRemote<ZeroStateSuggestionsClient>,
                 options: ZeroStateSuggestionsOptions,
  }): Promise<{suggestions?: ZeroStateSuggestionsV2}> {
    const {pendingRemote, options} = payload;
    const mojoOptions = {
      isFirstRun: options.isFirstRun ?? false,
      supportedTools: options.supportedTools ?? [],
    };

    this.clientReceiver?.$.close();
    this.clientReceiver = null;
    let mojoClientRemote = null;
    if (pendingRemote !== undefined) {
      const {receiver, remote} = createForwardingMojoRemote(
          this.router, pendingRemote, ZeroStateSuggestionsClientReceiver,
          ZeroStateSuggestionsClientImpl);
      this.clientReceiver = receiver;
      mojoClientRemote = remote;
    }
    return this.handler
        .getZeroStateSuggestionsAndSubscribe(mojoClientRemote, mojoOptions)
        .then((response: {
                zeroStateSuggestions: ZeroStateSuggestionsV2Mojo|null,
              }) => {
          if (!response.zeroStateSuggestions) {
            return {};
          }
          return {
            suggestions:
                zeroStateSuggestionsToClient(response.zeroStateSuggestions),
          };
        });
  }
}

export class ZeroStateSuggestionsClientImpl implements
    ZeroStateSuggestionsClientInterface {
  constructor(private sender: PostMessageRemote<ZeroStateSuggestionsClient>) {}

  notifyZeroStateSuggestionsChanged(
      suggestions: ZeroStateSuggestionsV2Mojo,
      options: ZeroStateSuggestionsOptionsMojo): void {
    this.sender.requestNoResponse('zeroStateSuggestionsChanged', {
      suggestions: zeroStateSuggestionsToClient(suggestions),
      options,
    });
  }
}
