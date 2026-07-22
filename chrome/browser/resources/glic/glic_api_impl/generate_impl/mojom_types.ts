// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Schema for the output of parse.py. Represents the mojom type
// information needed by gen_sources and gen_enum_conversions.

export interface MojomPrimitive {
  kind: 'primitive';
  name: string;
}

export interface MojomArray {
  kind: 'array';
  elementType: MojomType;
}

export interface MojomMap {
  kind: 'map';
  keyType: string;
  valueType: MojomType;
}

export interface MojomNamed {
  kind: 'named';
  name: string;
}

export type MojomType = MojomPrimitive|MojomArray|MojomMap|MojomNamed;

export interface MojomEnumValue {
  name: string;
  value: number;
  isDefault: boolean;
  comments: string[];
}

export interface MojomEnum {
  name: string;
  values: MojomEnumValue[];
  comments: string[];
}

export interface MojomField {
  name: string;
  mojomType: MojomType;
  isNullable: boolean;
  comments: string[];
}

export interface MojomStruct {
  name: string;
  fields: MojomField[];
  comments: string[];
  bodyComments?: string[];
}

export interface MojomUnion {
  name: string;
  fields: MojomField[];
  comments: string[];
}

export interface MojomModule {
  filename: string;
  imports: string[];
  enums: MojomEnum[];
  structs: MojomStruct[];
  unions: MojomUnion[];
  interfaces: unknown[];
}

export interface MojomAst {
  modules: MojomModule[];
}
