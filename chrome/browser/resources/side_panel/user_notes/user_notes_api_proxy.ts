// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {UserNotesPageHandler, UserNotesPageHandlerInterface} from './user_notes.mojom-webui.js';

let instance: UserNotesApiProxy|null = null;

export interface UserNotesApiProxy {
  showUi(): void;
}

export class UserNotesApiProxyImpl implements UserNotesApiProxy {
  handler: UserNotesPageHandlerInterface;

  constructor() {
    this.handler = new UserNotesPageHandler.getRemote();

    const factory = UserNotesPageHandler.getRemote();
    factory.createBookmarksPageHandler(
        this.handler.$.bindNewPipeAndPassReceiver());
  }

  showUi() {
    this.handler.showUI();
  }

  static getInstance(): UserNotesApiProxy {
    return instance || (instance = new UserNotesApiProxyImpl());
  }

  static setInstance(obj: UserNotesApiProxy) {
    instance = obj;
  }
}
