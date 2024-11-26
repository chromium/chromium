// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

export interface PrintOption<OptionValueType> {
  defaultValue?: OptionValueType;
  allowedValues?: OptionValueType[];
}

export interface Size {
  width: number;
  height: number;
}

export enum DuplexType {
  UNKNOWN_DUPLEX = 0,
  ONE_SIDED = 1,
  SHORT_EDGE = 2,
  LONG_EDGE = 3,
}

export interface Dpi {
  horizontal: number;
  vertical: number;
}

export enum QualityType {
  UNKNOWN_QUALITY = 0,
  DRAFT = 1,
  NORMAL = 2,
  HIGH = 3,
}

export interface ManagedPrintOptions {
  mediaSize?: PrintOption<Size>;
  mediaType?: PrintOption<string>;
  duplex?: PrintOption<DuplexType>;
  color?: PrintOption<boolean>;
  dpi?: PrintOption<Dpi>;
  quality?: PrintOption<QualityType>;
  printAsImage?: PrintOption<boolean>;
}
