// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {GraduationUiHandler, GraduationUiHandlerInterface} from '../mojom/graduation_ui.mojom-webui.js';

let graduationUiHandler: GraduationUiHandlerInterface|null;

export function getGraduationUiHandler(): GraduationUiHandlerInterface {
  if (!graduationUiHandler) {
    graduationUiHandler = GraduationUiHandler.getRemote();
  }
  return graduationUiHandler;
}

export function setGraduationUiHandlerForTesting(
    handler: GraduationUiHandlerInterface) {
  graduationUiHandler = handler;
}

export function resetGraduationHandlerForTesting() {
  graduationUiHandler = null;
}
