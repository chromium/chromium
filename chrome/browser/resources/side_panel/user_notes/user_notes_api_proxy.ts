// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {ClickModifiers} from 'chrome://resources/mojo/ui/base/mojom/window_open_disposition.mojom-webui.js';
import {Url} from 'chrome://resources/mojo/url/mojom/url.mojom-webui.js';

import {Note, NoteOverview, UserNotesPageCallbackRouter, UserNotesPageHandlerFactory, UserNotesPageHandlerRemote} from './user_notes.mojom-webui.js';

let instance: UserNotesApiProxy|null = null;

export interface UserNotesApiProxy {
  showUi(): void;
  getNoteOverviews(userInput: string): Promise<{overviews: NoteOverview[]}>;
  getNotesForCurrentTab(): Promise<{notes: Note[]}>;
  newNoteFinished(text: string): Promise<{success: boolean}>;
  updateNote(guid: string, text: string): Promise<{success: boolean}>;
  deleteNote(guid: string): Promise<{success: boolean}>;
  deleteNotesForUrl(url: Url): Promise<{success: boolean}>;
  noteOverviewSelected(url: Url, clickModifiers: ClickModifiers): void;
  setSortOrder(sortByNewest: boolean): void;
  hasNotesInAnyPages(): Promise<{hasNotes: boolean}>;
  openInNewTab(url: Url): void;
  openInNewWindow(url: Url): void;
  openInIncognitoWindow(url: Url): void;
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

  getNoteOverviews(userInput: string) {
    return this.handler.getNoteOverviews(userInput);
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

  deleteNotesForUrl(url: Url) {
    return this.handler.deleteNotesForUrl(url);
  }

  noteOverviewSelected(url: Url, clickModifiers: ClickModifiers) {
    this.handler.noteOverviewSelected(url, clickModifiers);
  }

  setSortOrder(sortByNewest: boolean) {
    this.handler.setSortOrder(sortByNewest);
  }

  hasNotesInAnyPages() {
    return this.handler.hasNotesInAnyPages();
  }

  openInNewTab(url: Url) {
    this.handler.openInNewTab(url);
  }

  openInNewWindow(url: Url) {
    this.handler.openInNewWindow(url);
  }

  openInIncognitoWindow(url: Url) {
    this.handler.openInIncognitoWindow(url);
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
