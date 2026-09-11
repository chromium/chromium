// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {ZeroStateSuggestionsClientReceiver, ZeroStateSuggestionsHandlerRemote} from '../../glic.mojom-webui.js';
import type {WebClientHandlerRemote, WebClientInitialState, ZeroStateSuggestionsClientInterface, ZeroStateSuggestionsOptions as ZeroStateSuggestionsOptionsMojo, ZeroStateSuggestionsV2 as ZeroStateSuggestionsV2Mojo} from '../../glic.mojom-webui.js';
import type {GlicBrowserHost, ObservableValue, ZeroStateSuggestionsOptions, ZeroStateSuggestionsV2} from '../../glic_api/glic_api.js';
import {ObservableValue as ObservableValueImpl} from '../../observable.js';
import {zeroStateSuggestionsToClient} from '../host/conversions.js';
import {maybeWrapWithLogging} from '../mojo_logging.js';

export class GlicBrowserHostZeroStateSuggestions implements
    ZeroStateSuggestionsClientInterface, Partial<GlicBrowserHost> {
  private handler?: ZeroStateSuggestionsHandlerRemote;
  private clientReceiver?: ZeroStateSuggestionsClientReceiver;

  currentZeroStateSuggestionOptions?: ZeroStateSuggestionsOptions;
  currentZeroStateObserver?: ObservableValueImpl<ZeroStateSuggestionsV2>;

  initialize(
      initialState: WebClientInitialState, handler: WebClientHandlerRemote) {
    if (!initialState.enableZeroStateSuggestions || !handler) {
      this.getZeroStateSuggestions = undefined;
      return;
    }

    this.handler = maybeWrapWithLogging(
        new ZeroStateSuggestionsHandlerRemote(),
        {prefix: 'ZeroStateSuggestionsHandler'});
    handler.createZeroStateSuggestionsHandler(
        this.handler.$.bindNewPipeAndPassReceiver());
  }

  destroySuggestions(): void {
    if (this.handler) {
      this.handler.$.close();
      this.handler = undefined;
    }
    if (this.clientReceiver) {
      this.clientReceiver.$.close();
      this.clientReceiver = undefined;
    }
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

  private async zeroStateActiveSubscriptionStateChanged(
      options: ZeroStateSuggestionsOptions,
      hasActiveSubscription: boolean): Promise<void> {
    if (options !== this.currentZeroStateSuggestionOptions) {
      // Don't send out of date updates.
      return;
    }

    if (!this.handler) {
      return;
    }

    this.clientReceiver?.$.close();
    this.clientReceiver = undefined;
    let clientRemote = null;
    if (hasActiveSubscription) {
      this.clientReceiver = new ZeroStateSuggestionsClientReceiver(this);
      clientRemote = this.clientReceiver.$.bindNewPipeAndPassRemote();
    }

    const mojoOptions: ZeroStateSuggestionsOptionsMojo = {
      isFirstRun: options.isFirstRun ?? false,
      supportedTools: options.supportedTools ?? [],
    };

    const response = await this.handler.getZeroStateSuggestionsAndSubscribe(
        clientRemote, mojoOptions);
    if (response.zeroStateSuggestions) {
      this.currentZeroStateObserver?.assignAndSignal(
          zeroStateSuggestionsToClient(response.zeroStateSuggestions));
    }
  }

  notifyZeroStateSuggestionsChanged(
      suggestions: ZeroStateSuggestionsV2Mojo,
      _options: ZeroStateSuggestionsOptionsMojo): void {
    this.currentZeroStateObserver?.assignAndSignal(
        zeroStateSuggestionsToClient(suggestions));
  }
}
