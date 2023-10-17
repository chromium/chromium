// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {AutocompleteControllerType, AutocompleteMatch, OmniboxPageCallbackRouter, OmniboxPageHandler, OmniboxPageHandlerRemote, OmniboxResponse, Signals} from '../omnibox.mojom-webui.js';

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

  getModelVersion(): Promise<number> {
    return this.handler.getMlModelVersion().then(({version}) => version);
  }

  makeMlRequest(signals: Signals): Promise<number> {
    return this.handler.startMl(signals).then(({score}) => score);
  }
}
