// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A simple schema utility to validate the JSON data in a
 * type-safe manner. The API is intentionally designed to work like a minimal
 * version of https://github.com/colinhacks/zod, focusing on JSON validation.
 */

import {assertInstanceof, checkEnumVariant} from './assert.js';

type KeyPath = string[];

interface Issue {
  schema: Schema<unknown>;
  path: KeyPath;
  message: string;
}

type IssueWithoutSchema = Omit<Issue, 'schema'>;

/**
 * A validation error is thrown when the schema fails to validate the input.
 */
export class ValidationError extends Error {
  constructor(readonly issue: Issue) {
    const {path, message} = issue;
    super(`${path.join('.')}: ${message}`);
    this.name = 'ValidationError';
  }
}

class Context {
  path: KeyPath = ['$'];

  issue: IssueWithoutSchema|null = null;

  pushKey(key: string) {
    this.path.push(key);
  }

  popKey() {
    this.path.pop();
  }

  setIssue(msg: string) {
    this.issue = {path: this.path.slice(), message: msg};
  }

  check(cond: boolean, msg: string): boolean {
    if (!cond) {
      // TODO(shik): Support passing a message creator function for expensive
      // messages.
      this.setIssue(msg);
    }
    return cond;
  }
}

/**
 * A schema is a type-safe class that can be used to validate the JSON data.
 * It can be used to validate the data in a type-safe manner.
 */
export class Schema<T> {
  constructor(readonly test: (input: unknown, ctx: Context) => input is T) {}

  parse(input: unknown): T {
    const ctx = new Context();
    if (this.test(input, ctx)) {
      return input;
    }
    // If the test failed, ctx.issue should be non-null. Adding a default value
    // in case it's missing.
    throw new ValidationError({
      schema: this,
      ...(ctx.issue ?? {path: ['$'], message: 'validation error'}),
    });
  }

  parseJson(input: string): T {
    const data = JSON.parse(input);
    return this.parse(data);
  }

  stringifyJson(val: T): string {
    return JSON.stringify(val);
  }
}

/**
 * A helper to get the corresponding type from schema.
 * TODO(shik): Find a way to export it as z.infer<T> instead of Infer<T> without
 * using namespace or extra files.
 */
export type Infer<T> = T extends Schema<infer U>? U : never;

function isNull(input: unknown, ctx: Context): input is null {
  return ctx.check(input === null, 'expect null');
}

function isBoolean(input: unknown, ctx: Context): input is boolean {
  return ctx.check(typeof input === 'boolean', 'expect boolean');
}

function isNumber(input: unknown, ctx: Context): input is number {
  return ctx.check(typeof input === 'number', 'expect number');
}

function isBigint(input: unknown, ctx: Context): input is bigint {
  return ctx.check(typeof input === 'bigint', 'expect bigint');
}

function isString(input: unknown, ctx: Context): input is string {
  return ctx.check(typeof input === 'string', 'expect string');
}

function isLiteral<T>(literal: T) {
  return (input: unknown, ctx: Context): input is T => {
    return ctx.check(input === literal, `expect ${literal}`);
  };
}

function isArray<T>(elem: Schema<T>) {
  return (input: unknown, ctx: Context): input is T[] => {
    if (!Array.isArray(input)) {
      ctx.setIssue('expect array');
      return false;
    }

    for (let i = 0; i < input.length; i++) {
      ctx.pushKey(`${i}`);
      if (!elem.test(input[i], ctx)) {
        return false;
      }
      ctx.popKey();
    }

    return true;
  };
}

// Used to expand the type from ObjectOutput<T> to make the type hint from
// language server more human-friendly.
type Expand<T> = T extends infer O ? {[K in keyof O]: O[K]} : never;
type ObjectSpec<T> = {
  [K in keyof T]: T[K] extends Schema<unknown>? T[K] : never;
};

// Mark all fields that accept undefined as optional. Note that there is no
// explicit `undefined` in JSON, so it's technically correct for JSON.
type IfOptional<T, Y, N> = undefined extends Infer<T>? Y : N;
type ObjectOutput<T> = {
  [K in keyof T as IfOptional<T[K], K, never>] +?: Infer<T[K]>;
}&{
  [K in keyof T as IfOptional<T[K], never, K>]: Infer<T[K]>;
};

function isObject<T>(spec: ObjectSpec<T>) {
  return (input: unknown, ctx: Context): input is Expand<ObjectOutput<T>> => {
    if (typeof input !== 'object') {
      ctx.setIssue('expect object');
      return false;
    }

    if (input === null) {
      ctx.setIssue('expect non-null object');
      return false;
    }

    for (const [key, schema] of Object.entries(spec)) {
      ctx.pushKey(key);
      // We're deliberately casting to access arbitrary key of the object.
      /* eslint-disable @typescript-eslint/consistent-type-assertions */
      const value = Object.hasOwn(input, key) ?
        (input as Record<string, unknown>)[key] :
        undefined;
      /* eslint-enable @typescript-eslint/consistent-type-assertions */
      if (!assertInstanceof(schema, Schema<unknown>).test(value, ctx)) {
        return false;
      }
      ctx.popKey();
    }

    return true;
  };
}

function isOptional<T>(schema: Schema<T>) {
  return (input: unknown, ctx: Context): input is T|undefined => {
    return input === undefined || schema.test(input, ctx);
  };
}

function isNullable<T>(schema: Schema<T>) {
  return (input: unknown, ctx: Context): input is T|null => {
    return input === null || schema.test(input, ctx);
  };
}

type SchemaArray = Array<Schema<unknown>>;

function isUnion<T extends SchemaArray>(schemas: T) {
  return (input: unknown, ctx: Context): input is Infer<T[number]> => {
    // TODO(shik): Expose the issues found in alternatives.
    const altCtx = new Context();
    if (!schemas.some((s) => s.test(input, altCtx))) {
      ctx.setIssue('expect union but all alternatives failed');
      return false;
    }
    return true;
  };
}

type SchemaListToIntersection<S> =
  S extends [Schema<infer Head>, ...infer Tail] ?
  Head&SchemaListToIntersection<Tail>:
  unknown;

function isIntersection<T extends SchemaArray>(schemas: T) {
  function cond(
    input: unknown,
    ctx: Context,
  ): input is SchemaListToIntersection<T> {
    // TODO(shik): Expose the issues found in alternatives.
    const altCtx = new Context();
    if (!schemas.every((s) => s.test(input, altCtx))) {
      ctx.setIssue('expect intersection but some alternatives failed');
      return false;
    }
    return true;
  }
  return cond;
}

type EnumObj = Record<string, string>;

// TODO(shik): Support numerical enums with bidirection mapping.
function isNativeEnum<T extends EnumObj>(enumObj: T) {
  return (input: unknown, ctx: Context): input is T[keyof T] => {
    return ctx.check(checkEnumVariant(enumObj, input) !== null, 'expect enum');
  };
}

/**
 * A minimal Zod-like interface.
 */
export const z = {
  'null': (): Schema<null> => new Schema(isNull),
  'boolean': (): Schema<boolean> => new Schema(isBoolean),
  'number': (): Schema<number> => new Schema(isNumber),
  'bigint': (): Schema<bigint> => new Schema(isBigint),
  'string': (): Schema<string> => new Schema(isString),
  'literal': <const T>(literal: T): Schema<T> => new Schema(isLiteral(literal)),
  'array': <T>(elem: Schema<T>): Schema<T[]> => new Schema(isArray(elem)),
  'object': <T>(spec: ObjectSpec<T>): Schema<Expand<ObjectOutput<T>>> =>
    new Schema(isObject(spec)),
  'optional': <T>(schema: Schema<T>): Schema<T|undefined> =>
    new Schema(isOptional(schema)),
  'nullable': <T>(schema: Schema<T>): Schema<T|null> =>
    new Schema(isNullable(schema)),
  'union': <T extends SchemaArray>(schemas: T): Schema<Infer<T[number]>> =>
    new Schema(isUnion(schemas)),
  'intersection': <T extends SchemaArray>(
    schemas: T,
  ): Schema<SchemaListToIntersection<T>> => new Schema(isIntersection(schemas)),
  'nativeEnum': <T extends EnumObj>(enumObj: T): Schema<T[keyof T]> =>
    new Schema(isNativeEnum(enumObj)),
};
