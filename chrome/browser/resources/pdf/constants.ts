// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

export interface Attachment {
  name: string;
  size: number;
  readable: boolean;
}

export enum DisplayAnnotationsAction {
  DISPLAY_ANNOTATIONS = 'display-annotations',
  HIDE_ANNOTATIONS = 'hide-annotations',
}

export interface DocumentMetadata {
  author: string;
  canSerializeDocument: boolean;
  creationDate: string;
  creator: string;
  fileSize: string;
  keywords: string;
  linearized: boolean;
  modDate: string;
  pageSize: string;
  producer: string;
  subject: string;
  title: string;
  version: string;
}

/** Enumeration of page fitting types. */
export enum FittingType {
  NONE = 'none',
  FIT_TO_PAGE = 'fit-to-page',
  FIT_TO_WIDTH = 'fit-to-width',
  FIT_TO_HEIGHT = 'fit-to-height',
}

export interface NamedDestinationMessageData {
  messageId: string;
  pageNumber: number;
  namedDestinationView?: string;
}

/**
 * Enumeration of save message request types. Must match `SaveRequestType` in
 * pdf/pdf_view_web_plugin.h.
 */
export enum SaveRequestType {
  ANNOTATION,
  ORIGINAL,
  EDITED,
}

export interface Point {
  x: number;
  y: number;
}

export type ExtendedKeyEvent = KeyboardEvent&{
  fromScriptingAPI?: boolean,
  fromPlugin?: boolean,
};
