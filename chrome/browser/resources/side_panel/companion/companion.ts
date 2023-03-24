// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './strings.m.js';

import {assert} from '//resources/js/assert_ts.js';
import {loadTimeData} from '//resources/js/load_time_data.js';

import {MethodType, PromoAction, PromoType} from './companion.mojom-webui.js';
import {CompanionProxy, CompanionProxyImpl} from './companion_proxy.js';

const companionProxy: CompanionProxy = CompanionProxyImpl.getInstance();

// Validation check for incoming enums from the iframe postMessage().
function validatePromoArguments(promoType: any, promoAction: any): boolean {
  const isValidType = Object.values(PromoType).includes(promoType);
  const isValidAction = Object.values(PromoAction).includes(promoAction);
  return isValidType && isValidAction;
}

function initialize() {
  // When the url is changed, we update our iframe src to pass new parameters.
  companionProxy.callbackRouter.onURLChanged.addListener((newUrl: string) => {
    const frame = document.body.querySelector('iframe');
    assert(frame);
    frame.src = newUrl;
  });

  companionProxy.handler.showUI();
}

// Handler for postMessage() calls from the embedded iframe.
function onCompanionMessageEvent(event: MessageEvent) {
  if (event.origin !== loadTimeData.getString('companion_origin')) {
    return;
  }

  const data = event.data;
  if (data.type === MethodType.kOnRegionSearchClicked) {
    companionProxy.handler.onRegionSearchClicked();
  } else if (data.type === MethodType.kOnPromoAction) {
    if (validatePromoArguments(data.promoType, data.promoAction)) {
      companionProxy.handler.onPromoAction(data.promoType, data.promoAction);
    }
  }
}

window.addEventListener('message', onCompanionMessageEvent, false);
document.addEventListener('DOMContentLoaded', initialize);
