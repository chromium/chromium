// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './strings.m.js';

import {assert} from '//resources/js/assert_ts.js';
import {loadTimeData} from '//resources/js/load_time_data.js';
import {Url} from '//resources/mojo/url/mojom/url.mojom-webui.js';

import {MethodType, PromoAction, PromoType} from './companion.mojom-webui.js';
import {CompanionProxy, CompanionProxyImpl} from './companion_proxy.js';

/**
 * Method arguments to be passed as part of the JSON message object to be sent
 * across the postmessage boundary.
 * Keep this file in sync with
 * google3/java/com/google/lens/web/interfaces/standalone/companionweb/service/companion_parent_communication_service.ts
 */
enum ParamType {
  // Arguments for iframe -> browser communication.
  // Mandatory arguments.
  METHOD_TYPE = 'type',

  // Optional arguments.
  // Arguments for MethodType.kOnExpsOptInStatusAvailable.
  IS_EXPS_OPTED_IN = 'isExpsOptedIn',

  // Arguments for MethodType.kOnPromoAction.
  PROMO_ACTION = 'promoAction',
  PROMO_TYPE = 'promoType',

  // Arguments for MethodType.kOnPhAction.
  PH_ACTION = 'phAction',

  // Arguments for MethodType.kOnOpenInNewTabButtonURLChanged.
  URL_FOR_OPEN_IN_NEW_TAB = 'urlForOpenInNewTab',

  // Arguments for browser -> iframe communcation.
  COMPANION_UPDATE_PARAMS = 'companion_update_params',
}

const companionProxy: CompanionProxy = CompanionProxyImpl.getInstance();

// Validation check for incoming enums from the iframe postMessage().
function validatePromoArguments(promoType: any, promoAction: any): boolean {
  const isValidType = Object.values(PromoType).includes(promoType);
  const isValidAction = Object.values(PromoAction).includes(promoAction);
  return isValidType && isValidAction;
}

function initialize() {
  // For the initial navigation, we update our iframe src to pass new
  // URL.
  companionProxy.callbackRouter.loadCompanionPage.addListener((newUrl: Url) => {
    const frame = document.body.querySelector('iframe');
    assert(frame);
    frame.src = newUrl.url;
  });

  // For subsequent navigations, we send a post message.
  companionProxy.callbackRouter.updateCompanionPage.addListener(
      (companionUpdateProto: string) => {
        const companionOrigin =
            new URL(loadTimeData.getString('companion_origin')).origin;
        const message =
            {[ParamType.COMPANION_UPDATE_PARAMS]: companionUpdateProto};

        const frame = document.body.querySelector('iframe');
        assert(frame);
        if (frame.contentWindow) {
          frame.contentWindow.postMessage(message, companionOrigin);
        }
      });

  companionProxy.handler.showUI();
}

// Handler for postMessage() calls from the embedded iframe.
function onCompanionMessageEvent(event: MessageEvent) {
  // Because the |companion_origin| string has a trailing slash that can cause
  // failures when doing a string comparison, convert the string to a URL and
  // compare the origin to prevent failures when origins are the same but
  // strings differ.
  const validOrigin =
      new URL(loadTimeData.getString('companion_origin')).origin;
  if (validOrigin !== event.origin) {
    return;
  }

  const data = event.data;
  const methodType = data[ParamType.METHOD_TYPE];
  if (methodType === MethodType.kOnRegionSearchClicked) {
    companionProxy.handler.onRegionSearchClicked();
  } else if (methodType === MethodType.kOnPromoAction) {
    const promoType = data[ParamType.PROMO_TYPE];
    const promoAction = data[ParamType.PROMO_ACTION];
    if (validatePromoArguments(promoType, promoAction)) {
      companionProxy.handler.onPromoAction(promoType, promoAction);
    }
  } else if (methodType === MethodType.kOnExpsOptInStatusAvailable) {
    companionProxy.handler.onExpsOptInStatusAvailable(
        data[ParamType.IS_EXPS_OPTED_IN]);
  }
}

window.addEventListener('message', onCompanionMessageEvent, false);
document.addEventListener('DOMContentLoaded', initialize);
