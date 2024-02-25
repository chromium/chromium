// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {PageHandlerInterface} from './hats.mojom-webui.js';
import {PageCallbackRouter, PageHandlerFactory, PageHandlerRemote} from './hats.mojom-webui.js';

type RequestSurveyFunction =
    (apiKey: string, triggerId: string, enableTesting: boolean,
     languageList: string[], productSpecificDataJson: string) => void;

class BrowserProxy {
  callbackRouter: PageCallbackRouter;
  handler: PageHandlerInterface;

  constructor(requestSurveyFn: RequestSurveyFunction) {
    this.callbackRouter = new PageCallbackRouter();
    this.callbackRouter.requestSurvey.addListener(requestSurveyFn);

    this.handler = new PageHandlerRemote();

    const factory = PageHandlerFactory.getRemote();
    factory.createPageHandler(
        this.callbackRouter.$.bindNewPipeAndPassRemote(),
        (this.handler as PageHandlerRemote).$.bindNewPipeAndPassReceiver());
  }
}

export {BrowserProxy};
