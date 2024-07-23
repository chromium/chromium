// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {ColorChangeUpdater} from '//resources/cr_components/color_change_listener/colors_css_updater.js';

import {ClientApi} from './boca_app.js';
import {CLIENT_DELEGATE} from './client_delegate.js';

/**
 * Returns the boca app if it can be found in the DOM.
 */
function getApp(): ClientApi {
  const app = document.querySelector('boca-app')!;
  return app as unknown as ClientApi;
}

/**
 * Runs any initialization code on the boca app once it is in the dom.
 */
function initializeApp(app: ClientApi) {
  app.setDelegate(CLIENT_DELEGATE);
}

/**
 * Called when a mutation occurs on document.body to check if the boca app is
 * available.
 */
function mutationCallback(
    _mutationsList: MutationRecord[], observer: MutationObserver) {
  const app = getApp();
  if (!app) {
    return;
  }
  // The boca app now exists so we can initialize it.
  initializeApp(app);
  observer.disconnect();
}

window.addEventListener('DOMContentLoaded', () => {
  // Start listening to color change events. These events get picked up by logic
  // in ts_helpers.ts on the google3 side.
  /** @suppress {checkTypes} */
  (function() {
    ColorChangeUpdater.forDocument().start();
  })();

  const app = getApp();
  if (app) {
    initializeApp(app);
    return;
  }
  // If translations need to be fetched, the app element may not be added yet.
  // In that case, observe <body> until it is.
  const observer = new MutationObserver(mutationCallback);
  observer.observe(document.body, {childList: true});

  ColorChangeUpdater.forDocument().start();
});
