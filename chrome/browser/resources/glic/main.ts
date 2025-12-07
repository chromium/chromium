// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '/strings.m.js';
import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_icon/cr_iconset.js';
import './icons.html.js';
import 'chrome://resources/cr_elements/icons.html.js';
import 'chrome://resources/cr_elements/cr_icon/cr_icon.js';
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import 'chrome://resources/cr_elements/cr_progress/cr_progress.js';

import {AppRouter} from './app_router.js';

declare global {
  interface Window {
    appRouter?: AppRouter;
  }
}


function setupListeners(controller: AppRouter) {
  // Allow WebUI close buttons to close the window.
  const buttons = document.querySelectorAll('.close-button');
  for (const button of buttons) {
    button.addEventListener('click', () => {
      controller.close();
    });
  }

  const retryButton = document.getElementById('retry');
  if (retryButton) {
    retryButton.addEventListener(
        'click',
        () => {
            // For now, do nothing. The button is inactive. The dialog will
            // disappear on its own if the network comes back online.
        });
  }

  const reloadButton = document.getElementById('reload');
  if (reloadButton) {
    reloadButton.addEventListener('click', () => {
      if ('reload' in controller) {
        controller.reload();
      }
    });
  }

  const debugButton = document.getElementById('debug');
  if (debugButton) {
    debugButton.addEventListener('click', () => {
      if ('showDebug' in controller) {
        controller.showDebug();
      }
    });
  }
}


window.addEventListener('DOMContentLoaded', () => {
  const appRouter = new AppRouter();
  window.appRouter = appRouter;
  setupListeners(appRouter);
});
