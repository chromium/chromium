// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {WebClientInitialState} from '../../glic.mojom-webui.js';
import type {GlicBrowserHost, ObservableValue, ZeroStateSuggestionsOptions, ZeroStateSuggestionsV2} from '../../glic_api/glic_api.js';
import {ObservableValue as ObservableValueImpl} from '../../observable.js';
import type {GlicBrowserHostBaseContext} from '../client/glic_client_common.js';
import type {PendingRemote, PostMessageHandler, PostMessageReceiver, PostMessageRemote} from '../transport/post_message_transport.js';

import {ZeroStateSuggestionsClientDef} from './zero_state_suggestions_types.js';
import type {ZeroStateSuggestionsClient, ZeroStateSuggestionsHost} from './zero_state_suggestions_types.js';

class ZeroStateSuggestionsClientImpl implements
    PostMessageHandler<ZeroStateSuggestionsClient> {
  constructor(private host: {
    currentZeroStateObserver?: ObservableValueImpl<ZeroStateSuggestionsV2>,
  }) {}

  zeroStateSuggestionsChanged(payload: {
    suggestions: ZeroStateSuggestionsV2,
    options: ZeroStateSuggestionsOptions,
  }): void {
    this.host.currentZeroStateObserver?.assignAndSignal(payload.suggestions);
  }
}

export class GlicBrowserHostZeroStateSuggestions implements
    Partial<GlicBrowserHost> {
  zeroStateSuggestionsSender?: PostMessageRemote<ZeroStateSuggestionsHost>;
  zeroStateSuggestionsReceiver: PostMessageReceiver|null = null;

  currentZeroStateSuggestionOptions?: ZeroStateSuggestionsOptions;
  currentZeroStateObserver?: ObservableValueImpl<ZeroStateSuggestionsV2>;

  constructor(private host: GlicBrowserHostBaseContext) {}

  initialize(
      initialState: WebClientInitialState,
      zeroStateSuggestionsRemote: PendingRemote<ZeroStateSuggestionsHost>|
      undefined) {
    if (zeroStateSuggestionsRemote === undefined ||
        !initialState.enableZeroStateSuggestions) {
      this.getZeroStateSuggestions = undefined;
      return;
    }

    this.zeroStateSuggestionsSender =
        this.host.router.newRemote(zeroStateSuggestionsRemote);
  }

  getZeroStateSuggestions?(options?: ZeroStateSuggestionsOptions):
      ObservableValue<ZeroStateSuggestionsV2> {
    options = options ?? {
      isFirstRun: false,
      supportedTools: [],
    };
    this.currentZeroStateSuggestionOptions = options;
    this.currentZeroStateObserver =
        ObservableValueImpl.withNoValue<ZeroStateSuggestionsV2>(
            this.zeroStateActiveSubscriptionStateChanged.bind(this, options));
    return this.currentZeroStateObserver;
  }

  async zeroStateActiveSubscriptionStateChanged(
      options: ZeroStateSuggestionsOptions, hasActiveSubscription: boolean) {
    if (options !== this.currentZeroStateSuggestionOptions) {
      // Dont send out of date updates.
      return;
    }

    let suggestionsClientPipe = undefined;
    if (hasActiveSubscription) {
      this.zeroStateSuggestionsReceiver?.close();
      const {remote, receiver} =
          this.host.router.newPipeWithReceiver<ZeroStateSuggestionsClient>(
              new ZeroStateSuggestionsClientImpl(this),
              ZeroStateSuggestionsClientDef);
      this.zeroStateSuggestionsReceiver = receiver;
      suggestionsClientPipe = remote;
    } else {
      this.zeroStateSuggestionsReceiver?.close();
      this.zeroStateSuggestionsReceiver = null;
    }

    const zeroStateResult =
        await this.zeroStateSuggestionsSender!.requestWithResponse(
            'getZeroStateSuggestionsAndSubscribe', {
              pendingRemote: suggestionsClientPipe,
              options: options,
            });
    if (zeroStateResult.suggestions) {
      this.currentZeroStateObserver?.assignAndSignal(
          zeroStateResult.suggestions);
    }
  }
}
