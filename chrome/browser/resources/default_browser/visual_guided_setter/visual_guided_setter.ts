// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '/strings.m.js';

import {ColorChangeUpdater} from 'chrome://resources/cr_components/color_change_listener/colors_css_updater.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.js';

import {PageCallbackRouter, PageHandlerFactory, PageHandlerRemote} from './visual_guided_setter.mojom-webui.js';
import type {PageHandlerInterface} from './visual_guided_setter.mojom-webui.js';

let handler: PageHandlerInterface|null = null;
let callbackRouter: PageCallbackRouter|null = null;

function updateAnchorRect() {
  const container = document.getElementById('illustration-container');
  if (!container || !handler) {
    return;
  }
  const rect = container.getBoundingClientRect();

  const left = Math.max(0, rect.left);
  const right = Math.min(window.innerWidth, rect.right);
  const top = Math.max(0, rect.top);
  const bottom = Math.min(window.innerHeight, rect.bottom);

  handler.setAnchorRect({
    x: Math.round(left),
    y: Math.round(top),
    width: Math.round(Math.max(0, right - left)),
    height: Math.round(Math.max(0, bottom - top)),
  });
}

function initialize() {
  ColorChangeUpdater.forDocument().start();

  // Hide the second step if the user cannot pin to taskbar.
  if (!loadTimeData.getBoolean('canPinToTaskbar')) {
    const instructions = document.getElementById('instructions');
    if (instructions) {
      instructions.hidden = true;
    }
  }

  const openSettingsLink = document.getElementById('open-settings-link');
  if (openSettingsLink) {
    openSettingsLink.addEventListener('click', (e) => {
      e.preventDefault();
      if (handler) {
        handler.openSettings();
      }
    });
  }

  callbackRouter = new PageCallbackRouter();
  callbackRouter.setErrorState.addListener((hasError: boolean) => {
    const container = document.getElementById('illustration-container');
    if (container) {
      container.classList.toggle('error', hasError);
    }
  });

  const handlerRemote = new PageHandlerRemote();
  handler = handlerRemote;

  PageHandlerFactory.getRemote().createPageHandler(
      callbackRouter.$.bindNewPipeAndPassRemote(),
      handlerRemote.$.bindNewPipeAndPassReceiver());

  const container = document.getElementById('illustration-container');
  if (container) {
    const observer = new ResizeObserver(() => {
      updateAnchorRect();
    });
    observer.observe(container);
  }

  window.addEventListener('resize', updateAnchorRect);
  updateAnchorRect();
}

document.addEventListener('DOMContentLoaded', initialize);
