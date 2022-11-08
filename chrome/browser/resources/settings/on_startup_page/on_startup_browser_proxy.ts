// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import {sendWithPromise} from 'chrome://resources/js/cr.js';
// clang-format on

export interface NtpExtension {
  id: string;
  name: string;
  canBeDisabled: boolean;
}

export interface OnStartupBrowserProxy {
  getNtpExtension(): Promise<NtpExtension|null>;
}

export class OnStartupBrowserProxyImpl implements OnStartupBrowserProxy {
  getNtpExtension() {
    return sendWithPromise('getNtpExtension');
  }

  static getInstance(): OnStartupBrowserProxy {
    return instance || (instance = new OnStartupBrowserProxyImpl());
  }

  static setInstance(obj: OnStartupBrowserProxy) {
    instance = obj;
  }
}

let instance: OnStartupBrowserProxy|null = null;
