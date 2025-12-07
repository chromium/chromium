// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// <if expr="enable_pdf_ink2">
export enum AnnotationMode {
  OFF = 'off',
  DRAW = 'draw',
  TEXT = 'text',
}

// The different types of annotation brushes.
export enum AnnotationBrushType {
  ERASER = 'eraser',
  HIGHLIGHTER = 'highlighter',
  PEN = 'pen',
}

export interface Color {
  r: number;
  g: number;
  b: number;
}

// The brush with parameters. Color and size are optional, since some brushes do
// not need them.
export interface AnnotationBrush {
  type: AnnotationBrushType;
  color?: Color;
  size?: number;
}

export interface TextAnnotation {
  id: number;
  pageNumber: number;
  text: string;
  textAttributes: TextAttributes;
  // Location of the text box relative to the top left corner of the page
  // specified by pageNumber. This rect is in screen coordinates in the UI,
  // and is in page coordinates when this annotation is sent or received in
  // a message to/from the plugin.
  textBoxRect: TextBoxRect;
  // Orientation of the text in the box relative to the PDF page, in number of
  // clockwise rotations from 0 to 3.
  textOrientation: number;
}

export enum TextAlignment {
  LEFT = 'left',
  CENTER = 'center',
  RIGHT = 'right',
}

export enum TextStyle {
  BOLD = 'bold',
  ITALIC = 'italic',
}

export enum TextTypeface {
  SANS_SERIF = 'sans-serif',
  SERIF = 'serif',
  MONOSPACE = 'monospace',
}

export type TextStyles = {
  [key in TextStyle]: boolean
};

export interface TextAttributes {
  typeface: TextTypeface;
  size: number;
  color: Color;
  alignment: TextAlignment;
  styles: TextStyles;
}

export interface TextBoxRect {
  height: number;
  locationX: number;
  locationY: number;
  width: number;
}
// </if> enable_pdf_ink2

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

/** Enumeration of page fitting types and bounding box fitting types. */
export enum FittingType {
  NONE = 'none',
  FIT_TO_PAGE = 'fit-to-page',
  FIT_TO_WIDTH = 'fit-to-width',
  FIT_TO_HEIGHT = 'fit-to-height',
  FIT_TO_BOUNDING_BOX = 'fit-to-bounding-box',
  FIT_TO_BOUNDING_BOX_WIDTH = 'fit-to-bounding-box-width',
  FIT_TO_BOUNDING_BOX_HEIGHT = 'fit-to-bounding-box-height',
}

/**
 * The different types of form fields that can be focused.
 */
export enum FormFieldFocusType {
  // LINT.IfChange(FocusFieldTypes)
  NONE = 'none',
  NON_TEXT = 'non-text',
  TEXT = 'text',
  // LINT.ThenChange(//pdf/pdf_view_web_plugin.cc:FocusFieldTypes)
}

export interface NamedDestinationMessageData {
  messageId: string;
  pageNumber: number;
  namedDestinationView?: string;
}

export interface Point {
  x: number;
  y: number;
}

export interface ScrollData extends Point {
  forceSmoothScroll: boolean;
}

export interface Rect {
  x: number;
  y: number;
  width: number;
  height: number;
}

export type ExtendedKeyEvent = KeyboardEvent&{
  fromScriptingAPI?: boolean,
  fromPlugin?: boolean,
};

// <if expr="enable_pdf_save_to_drive">
export enum SaveToDriveState {
  UNINITIALIZED = 'uninitialized',
  UPLOADING = 'uploading',
  SUCCESS = 'success',
  CONNECTION_ERROR = 'connection-error',
  STORAGE_FULL_ERROR = 'storage-full-error',
  SESSION_TIMEOUT_ERROR = 'session-timeout-error',
  UNKNOWN_ERROR = 'unknown-error',
}

export enum SaveToDriveBubbleRequestType {
  CANCEL_UPLOAD = 'cancel-upload',
  MANAGE_STORAGE = 'manage-storage',
  OPEN_IN_DRIVE = 'open-in-drive',
  RETRY = 'retry',
  DIALOG_CLOSED = 'dialog-closed',
}
// </if>
