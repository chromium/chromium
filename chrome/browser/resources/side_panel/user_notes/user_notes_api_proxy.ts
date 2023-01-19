// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {Note, UserNotesPageCallbackRouter, UserNotesPageHandlerFactory, UserNotesPageHandlerRemote} from './user_notes.mojom-webui.js';

let instance: UserNotesApiProxy|null = null;

export interface UserNotesApiProxy {
  showUi(): void;
  getNotesForCurrentTab(): Promise<{notes: Note[]}>;
  newNoteFinished(text: string): Promise<{success: boolean}>;
  updateNote(guid: string, text: string): Promise<{success: boolean}>;
  deleteNote(guid: string): Promise<{success: boolean}>;
  getCallbackRouter(): UserNotesPageCallbackRouter;
}

export class UserNotesApiProxyImpl implements UserNotesApiProxy {
  private callbackRouter: UserNotesPageCallbackRouter =
      new UserNotesPageCallbackRouter();
  private handler: UserNotesPageHandlerRemote =
      new UserNotesPageHandlerRemote();

  constructor() {
    const factory = UserNotesPageHandlerFactory.getRemote();
    factory.createPageHandler(
        this.callbackRouter.$.bindNewPipeAndPassRemote(),
        this.handler.$.bindNewPipeAndPassReceiver());
  }

  showUi() {
    this.handler.showUI();
  }

  getNotesForCurrentTab() {
    return this.handler.getNotesForCurrentTab();
  }

  newNoteFinished(text: string) {
    return this.handler.newNoteFinished(text);
  }

  updateNote(guid: string, text: string) {
    return this.handler.updateNote(guid, text);
  }

  deleteNote(guid: string) {
    return this.handler.deleteNote(guid);
  }

  getCallbackRouter() {
    return this.callbackRouter;
  }

  static getInstance(): UserNotesApiProxy {
    return instance || (instance = new UserNotesApiProxyImpl());
  }

  static setInstance(obj: UserNotesApiProxy) {
    instance = obj;
  }
}
