// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {DuplexType, QualityIppValue} from './cdd.js';

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

// Name of the IPP attribute that corresponds to the "quality" field in the
// managed print options.
export const IPP_PRINT_QUALITY: string = 'print-quality';

/**
 * Converts a ManagedPrintOptionsDuplexType value to a DuplexType value used in
 * CDD. Returns null if conversion is not possible.
 */
export function managedPrintOptionsDuplexToCdd(
    managedPrintOptionsDuplex: ManagedPrintOptionsDuplexType): DuplexType|null {
  switch (managedPrintOptionsDuplex) {
    case ManagedPrintOptionsDuplexType.ONE_SIDED:
      return DuplexType.NO_DUPLEX;
    case ManagedPrintOptionsDuplexType.LONG_EDGE:
      return DuplexType.LONG_EDGE;
    case ManagedPrintOptionsDuplexType.SHORT_EDGE:
      return DuplexType.SHORT_EDGE;
    default:
      return null;
  }
}

/**
 * Converts a ManagedPrintOptionsQualityType value to a common IPP value
 * represented by the QualityIppValue. Returns null if conversion is not
 * possible.
 */
export function managedPrintOptionsQualityToIpp(
    managedPrintOptionsQuality: ManagedPrintOptionsQualityType):
    QualityIppValue|null {
  switch (managedPrintOptionsQuality) {
    case ManagedPrintOptionsQualityType.DRAFT:
      return QualityIppValue.DRAFT;
    case ManagedPrintOptionsQualityType.NORMAL:
      return QualityIppValue.NORMAL;
    case ManagedPrintOptionsQualityType.HIGH:
      return QualityIppValue.HIGH;
    default:
      return null;
  }
}
