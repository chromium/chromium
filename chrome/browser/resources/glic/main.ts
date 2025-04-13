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

import {GlicAppController} from './glic_app_controller.js';

const appController = new GlicAppController();

declare global {
  interface Window {
    // For testing access.
    appController: GlicAppController;
  }
}
window.appController = appController;

window.addEventListener('load', () => {
  // Allow WebUI close buttons to close the window.
  const buttons = document.querySelectorAll('.close-button');
  for (const button of buttons) {
    button.addEventListener('click', () => {
      appController.close();
    });
  }
  document.getElementById('retry')!.addEventListener(
      'click',
      () => {
          // For now, do nothing. The button is inactive. The dialog will
          // disappear on its own if the network comes back online.
      });
  document.getElementById('reload')?.addEventListener('click', () => {
    appController.reload();
  });
  document.getElementById('debug')?.addEventListener('click', () => {
    appController.showDebug();
  });
});
