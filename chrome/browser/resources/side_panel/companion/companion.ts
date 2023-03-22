// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from '//resources/js/assert_ts.js';

import {CompanionProxy, CompanionProxyImpl} from './companion_proxy.js';

const companionProxy: CompanionProxy = CompanionProxyImpl.getInstance();


function initialize() {
  companionProxy.callbackRouter.onInitializePage.addListener(
      (initialUrl: string) => {
        const frame = document.body.querySelector('iframe');
        assert(frame);
        frame.src = initialUrl;
      });

  // When the url is changed, we update our iframe src to pass new parameters.
  companionProxy.callbackRouter.onURLChanged.addListener((newUrl: string) => {
    const frame = document.body.querySelector('iframe');
    assert(frame);
    frame.src = newUrl;
  });

  companionProxy.handler.showUI();
}

document.addEventListener('DOMContentLoaded', initialize);
