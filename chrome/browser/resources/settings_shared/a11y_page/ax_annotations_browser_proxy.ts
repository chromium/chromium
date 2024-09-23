// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {sendWithPromise} from 'chrome://resources/js/cr.js';

/**
 * Numerical values should not be changed because they must stay in sync with
 * screen_ai::ScreenAIInstallState::State defined in screen_ai_install_state.h.
 */
export enum ScreenAiInstallStatus {
  NOT_DOWNLOADED = 0,
  DOWNLOADING = 1,
  DOWNLOAD_FAILED = 2,
  DOWNLOADED = 3,
}

export interface AxAnnotationsBrowserProxy {
  /**
   * Requests the install state of ScreenAI.
   */
  getScreenAiInstallState(): Promise<ScreenAiInstallStatus>;
}

let instance: AxAnnotationsBrowserProxy|null = null;

export class AxAnnotationsBrowserProxyImpl implements
    AxAnnotationsBrowserProxy {
  static getInstance(): AxAnnotationsBrowserProxy {
    return instance || (instance = new AxAnnotationsBrowserProxyImpl());
  }

  static setInstanceForTesting(obj: AxAnnotationsBrowserProxy): void {
    instance = obj;
  }

  getScreenAiInstallState(): Promise<ScreenAiInstallStatus> {
    return sendWithPromise('getScreenAiInstallState');
  }
}
