// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {MojomType} from './mojom_types.js';

// Types representing typescript types.

export const BUILTIN_TS_TYPES = new Set<string>(
    ['string', 'boolean', 'number', 'any', 'ArrayBuffer', 'Uint8Array']);

export abstract class TsType {
  abstract toString(): string;
  typeParams: TsType[] = [];
}

export class TsNamedType extends TsType {
  name: string;
  constructor(name: string, typeParams: TsType[] = []) {
    super();
    this.name = name;
    this.typeParams = typeParams;
  }
  override toString(): string {
    if (this.typeParams.length > 0) {
      return `${this.name}<${
          this.typeParams.map(p => p.toString()).join(', ')}>`;
    }
    return this.name;
  }
}

export class TsArrayType extends TsType {
  elementType: TsType;
  constructor(elementType: TsType) {
    super();
    this.elementType = elementType;
  }
  override toString(): string {
    return `${this.elementType.toString()}[]`;
  }
}

export function parseTsType(typeStr: string): TsType {
  typeStr = typeStr.trim();
  if (typeStr.endsWith('[]')) {
    return new TsArrayType(parseTsType(typeStr.slice(0, -2)));
  }
  if (typeStr.includes('<')) {
    const start = typeStr.indexOf('<');
    const end = typeStr.lastIndexOf('>');
    const name = typeStr.slice(0, start).trim();
    const paramsStr = typeStr.slice(start + 1, end).trim();
    // Assuming single param for now as in Python ParseTsType
    return new TsNamedType(name, [parseTsType(paramsStr)]);
  }
  return new TsNamedType(typeStr);
}

export interface TsFieldType {
  type: TsType;
  isOptional?: boolean;
}
export interface TsField {
  name: string;
  comments?: string[];
  typeInfo: TsFieldType;
}
export interface TsEnumValue {
  name: string;
  value: number;
  comments?: string[];
}
export interface TsEnum {
  name: string;
  comments?: string[];
  values: TsEnumValue[];
}
export interface TsInterface {
  name: string;
  comments?: string[];
  fields: TsField[];
}

//
// Types extending typescript types, for types that are sourced
// from mojom.
//

export interface TypeInfo extends TsFieldType {
  mojomType: MojomType;
  // The mojom field was nullable.
  isNullable?: boolean;
}

export interface Field extends TsField {
  typeInfo: TypeInfo;
  // The original name of the field in mojom.
  mojomName: string;
  // Whether the field should be ignored.
  isIgnored?: boolean;
}

export interface Enum extends TsEnum {
  // Mojom source file for the enum.
  mojomFile: string;
}

export interface Interface extends TsInterface {
  fields: Field[];
  // Whether this is a union.
  isUnion?: boolean;
  // Mojom source file for the struct/union.
  mojomFile: string;
}

export function withoutIgnored(i: Interface): TsInterface {
  return {
    name: i.name,
    comments: i.comments,
    fields: i.fields.filter(f => !f.isIgnored),
  };
}
