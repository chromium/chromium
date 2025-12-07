// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {AutocompleteControllerType, AutocompleteMatch, OmniboxPageHandlerRemote, OmniboxResponse, Signals} from '../omnibox_internals.mojom-webui.js';
import {OmniboxPageCallbackRouter, OmniboxPageHandler} from '../omnibox_internals.mojom-webui.js';
import {MlVersionObj} from '../omnibox_util.js';

export enum ResponseFilter {
  FINAL = 'Final results',
  ALL = 'All results',
}

type OnResponseCallback =
    (responseFilter: ResponseFilter, controllerType: AutocompleteControllerType,
     input: string, matches: AutocompleteMatch[]) => void;

export class MlBrowserProxy {
  private readonly callbackRouter: OmniboxPageCallbackRouter =
      new OmniboxPageCallbackRouter();
  private readonly handler: OmniboxPageHandlerRemote =
      OmniboxPageHandler.getRemote();
  private onResponseCallbacks: OnResponseCallback[] = [];
  private version: Promise<MlVersionObj>;
  private readonly makeMlRequestCache: Record<string, Promise<number>> = {};

  constructor() {
    this.callbackRouter.handleNewAutocompleteResponse.addListener(
        this.handleNewAutocompleteResponse.bind(this));
    this.callbackRouter.handleNewMlResponse.addListener(
        this.handleNewMlResponse.bind(this));
    this.handler.setClientPage(
        this.callbackRouter.$.bindNewPipeAndPassRemote());
  }

  addResponseListener(callback: OnResponseCallback) {
    this.onResponseCallbacks.push(callback);
  }

  private onResponse(
      responseFilter: ResponseFilter,
      controllerType: AutocompleteControllerType, input: string,
      matches: AutocompleteMatch[]) {
    this.onResponseCallbacks.forEach(
        callback => callback(responseFilter, controllerType, input, matches));
  }

  private handleNewAutocompleteResponse(
      controllerType: AutocompleteControllerType, response: OmniboxResponse) {
    this.onResponse(
        ResponseFilter.FINAL, controllerType, response.inputText,
        response.combinedResults);
  }

  private handleNewMlResponse(
      controllerType: AutocompleteControllerType, input: string,
      matches: AutocompleteMatch[]) {
    this.onResponse(ResponseFilter.ALL, controllerType, input, matches);
  }

  get modelVersion(): Promise<MlVersionObj> {
    return this.version ||= this.handler.getMlModelVersion().then(
               ({version}) => new MlVersionObj(version));
  }

  makeMlRequest(signals: Signals): Promise<number> {
    const cacheKey = String(Object.values(signals));
    return this.makeMlRequestCache[cacheKey] ||=
               this.handler.startMl(signals).then(({score}) => score);
  }
}
