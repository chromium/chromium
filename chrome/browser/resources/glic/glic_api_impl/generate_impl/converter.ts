// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Converts raw Mojom AST modules into intermediate TypeScript
 * model definitions.
 *
 * This stage parses raw Mojom types, structs, unions, and interfaces into
 * high-level intermediate TypeScript models (`Model`, `Interface`, `Enum`,
 * `BridgedInterface`). It handles:
 * - Parsing `@glic_*` comment annotations (e.g., `@glic_type`,
 * `@glic_optional`, `@glic_ignore`, `@glic_field`).
 * - Mapping Mojom C++ primitive and system types to client-facing TypeScript
 * types.
 * - Resolving custom enum value mappings, public vs. private types, and
 * optional default annotations.
 */

import type {MojomEnum, MojomField, MojomModule, MojomStruct, MojomType, MojomUnion} from './mojom_types.ts';
import {parseTsType} from './ts_types.ts';
import type {Enum, Field, Interface, TsEnumValue} from './ts_types.ts';

export const ENUM_MAPPINGS: Record<string, Record<string, string|null>> = {
  'WebClientMode': {'kUnknown': null},
  'SettingsPageField': {'kNone': null},
  'PerformActionsErrorReason':
      {'kMissingTaskId': null, 'kInvalidProto': 'INVALID_ACTION_PROTO'},
};

export const TYPE_MAPPINGS: Record<string, string> = {
  'string': 'string',
  'bool': 'boolean',
  'double': 'number',
  'float': 'number',
  'int8': 'number',
  'int16': 'number',
  'int32': 'number',
  'int64': 'number',
  'uint8': 'number',
  'uint16': 'number',
  'uint32': 'number',
  'uint64': 'number',
  'url.mojom.Url': 'string',
  'url.mojom.Origin': 'string',
  'mojo_base.mojom.UnguessableToken': 'string',
  'SafeBrowsingVerdictResult': 'SafeBrowsingVerdict',
  'mojo_base.mojom.ByteString': 'string',
  'gfx.mojom.Rect': 'Rect',
  'gfx.mojom.Point': 'Point',
};

export const STRINGLIKE_ID_RE = /(tab|window)(_i|I)d$/;

export function snakeToCamelCase(s: string): string {
  return s.replace(
      /([^_])_+([a-z0-9])/gi, (_, c1, c2) => c1 + c2.toUpperCase());
}

export function getMojoImportFile(filename: string): string {
  if (!filename) {
    return '';
  }
  const parts = filename.split('/');
  const base = parts[parts.length - 1]!;
  return base.replace('.mojom', '.mojom-webui.js');
}

interface ParsedComments {
  cleanComments: string[];
  generateApi: boolean;
  isOptional: boolean;
  isIgnored: boolean;
  typeOverride: string|null;
}

function parseComments(rawComments: string[]): ParsedComments {
  let generateApi = false;
  let isOptional = false;
  let isIgnored = false;
  let typeOverride: string|null = null;
  const cleanComments: string[] = [];

  for (const comment of rawComments) {
    const line = comment.trim();

    if (line.includes('@generate glic_api')) {
      generateApi = true;
      cleanComments.length = 0;  // Trim comments above @generate glic_api
      continue;
    }

    const typeMatch = line.match(/@glic_type\s+(\S+)/);
    if (typeMatch && typeMatch[1]) {
      typeOverride = typeMatch[1];
      if (typeOverride.endsWith('?')) {
        typeOverride = typeOverride.slice(0, -1);
        isOptional = true;
      }
      continue;
    }

    if (line.includes('@glic_optional')) {
      isOptional = true;
      continue;
    }

    if (line.includes('@glic_ignore')) {
      isIgnored = true;
      continue;
    }

    // Ignore lint and version comments
    const isLintComment =
        new RegExp('.*([L]INT.IfChange|[L]INT.ThenChange)').test(line);
    if (isLintComment ||
        /\/\/\s*\/\/(components|chrome|tools|ui)\/.*/.test(line) ||
        /\/\/\s*Next version:/.test(line) || line.startsWith('///')) {
      continue;
    }

    cleanComments.push(line);
  }

  return {
    cleanComments,
    generateApi,
    isOptional,
    isIgnored,
    typeOverride,
  };
}

export class MojomModel {
  generatedNames = new Set<string>();
  generatedInterfaces = new Set<string>();
  generatedEnums = new Set<string>();
  referencedTypes = new Set<string>();
  enums: Enum[] = [];
  interfaces: Interface[] = [];
  convertedEnums: Array<[string, string]> = [];

  modules: MojomModule[];
  constructor(modules: MojomModule[]) {
    this.modules = modules;
    this.convertEnums();
    this.convertStructs();
    this.convertUnions();
  }

  private lookupName(name: string, remap: Record<string, string|null>): string {
    if (!(name in remap)) {
      return name;
    }
    const val = remap[name]!;
    return val === null ? name : val;
  }

  private convertEnums(remappings = ENUM_MAPPINGS) {
    this.convertedEnums = [];
    for (const module of this.modules) {
      let source = 'glic';
      if (module.filename.includes('actor_webui.mojom')) {
        source = 'actor';
      } else if (module.filename.includes('glic_enums.mojom')) {
        source = 'glic_enums';
      }
      for (const e of module.enums) {
        const parsed = parseComments(e.comments);
        if (parsed.generateApi) {
          this.convertEnum(
              e, parsed.cleanComments, remappings[e.name] || {},
              module.filename);
          this.convertedEnums.push([e.name, source]);
        }
      }
    }
  }

  private convertEnum(
      enumNode: MojomEnum, cleanComments: string[],
      remap: Record<string, string|null>, filename = '') {
    const enumName = enumNode.name;
    this.generatedNames.add(enumName);
    this.generatedEnums.add(enumName);
    const localRemap = {...remap};
    const values: TsEnumValue[] = [];
    for (const v of enumNode.values) {
      if (v.name in localRemap && localRemap[v.name] === null) {
        delete localRemap[v.name];
        continue;
      }
      const valueName = this.lookupName(v.name, localRemap);
      if (v.name in localRemap) {
        delete localRemap[v.name];
      }
      const parsedVal = parseComments(v.comments);
      values.push(
          {name: valueName, value: v.value, comments: parsedVal.cleanComments});
    }
    if (Object.keys(localRemap).length > 0) {
      throw new Error(
          `Unused remap for ${enumName}: ${JSON.stringify(localRemap)}`);
    }
    this.enums.push({
      name: enumName,
      comments: cleanComments,
      values,
      mojomFile: getMojoImportFile(filename),
    });
  }

  private convertStructs(typeMappings = TYPE_MAPPINGS) {
    for (const module of this.modules) {
      for (const s of module.structs) {
        const parsed = parseComments(s.comments);
        if (parsed.generateApi) {
          this.convertStruct(
              s, parsed.cleanComments, typeMappings, module.filename);
        }
      }
    }
  }

  private convertStruct(
      structNode: MojomStruct, cleanComments: string[],
      typeMappings: Record<string, string>, filename = '') {
    const structName = structNode.name;
    this.generatedNames.add(structName);
    this.generatedInterfaces.add(structName);
    const fields = this.convertFields(
        structName, 'struct', structNode.fields, typeMappings);
    this.interfaces.push({
      name: structName,
      comments: cleanComments,
      fields,
      isUnion: false,
      mojomFile: getMojoImportFile(filename),
    });
  }

  private convertUnions(typeMappings = TYPE_MAPPINGS) {
    for (const module of this.modules) {
      for (const u of module.unions) {
        const parsed = parseComments(u.comments);
        if (parsed.generateApi) {
          this.convertUnion(
              u, parsed.cleanComments, typeMappings, module.filename);
        }
      }
    }
  }

  private convertUnion(
      unionNode: MojomUnion, cleanComments: string[],
      typeMappings: Record<string, string>, filename = '') {
    const unionName = unionNode.name;
    this.generatedNames.add(unionName);
    this.generatedInterfaces.add(unionName);
    const fields = this.convertFields(
        unionName, 'union', unionNode.fields, typeMappings, true);
    this.interfaces.push({
      name: unionName,
      comments: cleanComments,
      fields,
      isUnion: true,
      mojomFile: getMojoImportFile(filename),
    });
  }

  private mapMojomTypeToTs(
      mojomType: MojomType, typeMappings: Record<string, string>): string|null {
    if (mojomType.kind === 'array') {
      const itemType =
          this.mapMojomTypeToTs(mojomType.elementType, typeMappings);
      return itemType ? `${itemType}[]` : null;
    }
    if (mojomType.kind === 'map') {
      const keyTs = this.mapMojomTypeToTs(
          {kind: 'primitive', name: mojomType.keyType}, typeMappings);
      const valTs = this.mapMojomTypeToTs(mojomType.valueType, typeMappings);
      if (keyTs && valTs) {
        return `Record<${keyTs}, ${valTs}>`;
      }
      return null;
    }
    if (mojomType.kind !== 'named' && mojomType.kind !== 'primitive') {
      return null;
    }

    const typeStr = mojomType.name;
    let mappedType = typeStr;
    if (typeStr in typeMappings) {
      mappedType = typeMappings[typeStr]!;
    } else {
      const parts = typeStr.split('.');
      mappedType = parts[parts.length - 1]!;
    }

    // Check if it is alphanumeric (equivalent to isalnum in Python)
    if (/^[a-zA-Z0-9]+$/.test(mappedType)) {
      if (!['string', 'boolean', 'number', 'any', 'void'].includes(
              mappedType)) {
        this.referencedTypes.add(mappedType);
      }
      return mappedType;
    }
    return null;
  }

  private mapMojomFieldTypeToTs(
      field: MojomField, typeMappings: Record<string, string>): string|null {
    const mojomType = field.mojomType;
    const isInt32 =
        mojomType.kind === 'primitive' && mojomType.name === 'int32';
    if (isInt32 && STRINGLIKE_ID_RE.test(field.name)) {
      return 'string';
    }
    return this.mapMojomTypeToTs(mojomType, typeMappings);
  }

  private convertFields(
      containerName: string, containerType: string, fields: MojomField[],
      typeMappings: Record<string, string>, forceOptional = false): Field[] {
    const convertedFields: Field[] = [];
    for (const field of fields) {
      const parsed = parseComments(field.comments);
      let tsType: string|null = null;
      let isOptional = parsed.isOptional;
      const isIgnored = parsed.isIgnored;

      if (parsed.typeOverride) {
        tsType = parsed.typeOverride;
      }

      if (!tsType) {
        if (isIgnored) {
          tsType = 'any';
        } else {
          tsType = this.mapMojomFieldTypeToTs(field, typeMappings);
        }
      }

      if (!tsType) {
        throw new Error(
            `Unsupported Mojo type '${
                JSON.stringify(field.mojomType)}' for field` +
            ` '${field.name}' in ${containerType} '${containerName}'`);
      }

      const fieldName = snakeToCamelCase(field.name);
      isOptional = forceOptional || field.isNullable || isOptional;
      convertedFields.push({
        name: fieldName,
        comments: parsed.cleanComments,
        typeInfo: {
          type: parseTsType(tsType),
          mojomType: field.mojomType,
          isOptional: isOptional,
          isNullable: field.isNullable,
        },
        mojomName: field.name,
        isIgnored: isIgnored,
      });
    }
    return convertedFields;
  }
}
