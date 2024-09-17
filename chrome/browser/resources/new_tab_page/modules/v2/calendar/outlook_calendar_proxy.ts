// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {OutlookCalendarPageHandlerRemote} from '../../../outlook_calendar.mojom-webui.js';
import {OutlookCalendarPageHandler} from '../../../outlook_calendar.mojom-webui.js';

export interface OutlookCalendarProxy {
  handler: OutlookCalendarPageHandlerRemote;
}

export class OutlookCalendarProxyImpl implements OutlookCalendarProxy {
  handler: OutlookCalendarPageHandlerRemote;

  constructor(handler: OutlookCalendarPageHandlerRemote) {
    this.handler = handler;
  }

  static getInstance(): OutlookCalendarProxy {
    return instance ||
        (instance = new OutlookCalendarProxyImpl(
             OutlookCalendarPageHandler.getRemote()));
  }

  static setInstance(newInstance: OutlookCalendarProxy) {
    instance = newInstance;
  }
}

let instance: OutlookCalendarProxy|null = null;
