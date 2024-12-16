// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

export interface PrintOption<OptionValueType> {
  defaultValue?: OptionValueType;
  allowedValues?: OptionValueType[];
}

export interface ManagedPrintOptionsSize {
  width: number;
  height: number;
}

export enum ManagedPrintOptionsDuplexType {
  UNKNOWN_DUPLEX = 0,
  ONE_SIDED = 1,
  SHORT_EDGE = 2,
  LONG_EDGE = 3,
}

export interface ManagedPrintOptionsDpi {
  horizontal: number;
  vertical: number;
}

export enum ManagedPrintOptionsQualityType {
  UNKNOWN_QUALITY = 0,
  DRAFT = 1,
  NORMAL = 2,
  HIGH = 3,
}

export interface ManagedPrintOptions {
  mediaSize?: PrintOption<ManagedPrintOptionsSize>;
  mediaType?: PrintOption<string>;
  duplex?: PrintOption<ManagedPrintOptionsDuplexType>;
  color?: PrintOption<boolean>;
  dpi?: PrintOption<ManagedPrintOptionsDpi>;
  quality?: PrintOption<ManagedPrintOptionsQualityType>;
  printAsImage?: PrintOption<boolean>;
}
