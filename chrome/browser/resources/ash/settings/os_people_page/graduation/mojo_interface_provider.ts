// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {GraduationHandler, GraduationHandlerInterface} from '../../mojom-webui/graduation_handler.mojom-webui.js';

let testHandlerProvider: GraduationHandlerInterface|null = null;

export function setGraduationHandlerProviderForTesting(
    testProvider: GraduationHandlerInterface): void {
  testHandlerProvider = testProvider;
}

export function getGraduationHandlerProvider(): GraduationHandlerInterface {
  // For testing only.
  if (testHandlerProvider) {
    return testHandlerProvider;
  }
  return GraduationHandler.getRemote();
}
