// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// <if expr="enable_pdf_ink2">
type Typeface = chrome.pdfViewerPrivate.Typeface;

export enum AnnotationMode {
  OFF = 'off',
  DRAW = 'draw',
  TEXT = 'text',
  SIGNATURE = 'signature',
}

// The different types of annotation brushes.
export enum AnnotationBrushType {
  ERASER = 'eraser',
  HIGHLIGHTER = 'highlighter',
  PEN = 'pen',
}

export enum TextAnnotationSource {
  USER = 'user',
  UNDO = 'undo',
  REDO = 'redo',
}

export interface Color {
  r: number;
  g: number;
  b: number;
}

export interface ColorOption {
  label: string;
  color: string;
  blended: boolean;
}

export const TEXT_AND_SIGNATURE_COLORS: ColorOption[] = [
  // LINT.IfChange(TextAnnotationColors)
  // Row 1:
  {label: 'annotationColorBlack', color: '#000000', blended: false},
  {label: 'ink2BrushColorDarkGrey2', color: '#5f6368', blended: false},
  {label: 'ink2BrushColorDarkGrey1', color: '#9aa0a6', blended: false},
  {label: 'annotationColorLightGrey', color: '#dadce0', blended: false},
  {label: 'annotationColorWhite', color: '#ffffff', blended: false},
  // Row 2:
  {label: 'ink2BrushColorRed1', color: '#f28b82', blended: false},
  {label: 'ink2BrushColorYellow1', color: '#fdd663', blended: false},
  {label: 'ink2BrushColorGreen1', color: '#81c995', blended: false},
  {label: 'ink2TextColorCyan1', color: '#78d9ec', blended: false},
  {label: 'ink2BrushColorBlue1', color: '#8ab4f8', blended: false},
  // Row 3:
  {label: 'ink2BrushColorRed2', color: '#e94235', blended: false},
  {label: 'ink2BrushColorYellow2', color: '#fbbc04', blended: false},
  {label: 'ink2BrushColorGreen2', color: '#34a853', blended: false},
  {label: 'ink2TextColorCyan2', color: '#24c1e0', blended: false},
  {label: 'ink2BrushColorBlue2', color: '#4285f4', blended: false},
  // Row 4:
  {label: 'ink2BrushColorRed3', color: '#c5221f', blended: false},
  {label: 'ink2BrushColorYellow3', color: '#d56e0c', blended: false},
  {label: 'ink2BrushColorGreen3', color: '#188038', blended: false},
  {label: 'ink2TextColorCyan3', color: '#12a4af', blended: false},
  {label: 'ink2BrushColorBlue3', color: '#1967d2', blended: false},
  // LINT.ThenChange(//pdf/pdf_ink_metrics_handler.cc:TextAnnotationColors)
];

// The brush with parameters. Color and size are optional, since some brushes do
// not need them.
export interface AnnotationBrush {
  type: AnnotationBrushType;
  color?: Color;
  size?: number;
}

export interface DeleteTextAnnotationMessageData {
  id: number;
  pageIndex: number;
  source: TextAnnotationSource;
}

export interface TextAnnotation {
  id: number;
  // Stored because the backend requires it to be re-sent with every update.
  // Not used by frontend code.
  mojoTextInfo: ArrayBuffer;
  pageIndex: number;
  // Zoom level at TextAnnotation creation time. Stored for use by the backend
  // on every update. Not used by frontend code.
  pdfZoom: number;
  text: string;
  textAttributes: TextAttributes;
  // Location of the text box relative to the top left corner of the page
  // specified by pageIndex. This rect is in screen coordinates in the UI,
  // and is in page coordinates when this annotation is sent or received in
  // a message to/from the plugin.
  textBoxRect: TextBoxRect;
  // Orientation of the text in the box relative to the PDF page, in number of
  // clockwise rotations from 0 to 3.
  textOrientation: number;
  // Orientation of the viewport when the annotation was committed, in number of
  // clockwise rotations from 0 to 3.
  viewportOrientation: number;
}

export interface TextAnnotationMessageData extends TextAnnotation {
  isEdited: boolean;
  // Serialized SkTypeface font data that the backend needs. Only contains
  // fonts that the backend has never seen before.
  newTypefaces: Typeface[];
  source: TextAnnotationSource;
}

// LINT.IfChange(TextAlignment)
export enum TextAlignment {
  LEFT = 'left',
  CENTER = 'center',
  RIGHT = 'right',
}
// LINT.ThenChange(//pdf/pdf_ink_text.h:TextAlignment)

export enum TextStyle {
  BOLD = 'bold',
  ITALIC = 'italic',
  UNDERLINE = 'underline',
  STRIKETHROUGH = 'strikethrough',
}

// LINT.IfChange(TextTypeface)
export enum TextTypeface {
  SANS_SERIF = 'sans-serif',
  SERIF = 'serif',
  MONOSPACE = 'monospace',
}
// LINT.ThenChange(//pdf/pdf_ink_text.h:TextTypeface)

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
  pageIndex: number;
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
