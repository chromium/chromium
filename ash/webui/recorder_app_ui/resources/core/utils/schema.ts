// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A simple schema utility to validate the JSON data in a
 * type-safe manner. The API is intentionally designed to work like a minimal
 * version of mix of https://github.com/gcanti/io-ts and
 * https://github.com/colinhacks/zod, focusing on JSON validation and encode.
 *
 * TODO(pihsun): There are a LOT of type assertions in this file, it'd be very
 * helpful to have some unit tests...
 */

import {assertExists, assertNotReached, checkEnumVariant} from './assert.js';

type KeyPath = string[];

// Used as a base schema type to accept any Schema. We can't use `unknown`
// here because of the type of Schema (encode use the `Output` type as
// argument).
// eslint-disable-next-line @typescript-eslint/no-explicit-any
type AnySchema = Schema<any, unknown>;

interface Issue {
  // The schema is only used to do identity comparison on which parse failed
  // in global error handler.
  schema: AnySchema;
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
}

const DECODE_ERROR = Symbol('SCHEMA_DECODE_ERROR');
type Maybe<T> = T|typeof DECODE_ERROR;

/**
 * A schema is a type-safe class that can be used to validate the JSON data.
 * It can be used to validate the data in a type-safe manner.
 *
 * The schema represents a value of type `Output`, that can check and decode
 * from a value of type `Input`, and be encoded back to type `Input`.
 */
export class Schema<Output, Input = Output> {
  /**
   * Tests if the input is of type `Output`.
   */
  readonly test: (input: unknown) => input is Output;

  /**
   * Decodes an value to type `Output`.
   *
   * Decoding would success only when the input is of type `Input`, otherwise
   * returns `DECODE_ERROR`.
   */
  readonly decode: (input: unknown, ctx: Context) => Maybe<Output>;

  /**
   * Encodes the value to type `Input`.
   *
   * This should be the inverse of decode, so `decode(encode(val))` should
   * always be the same as `val`.
   */
  readonly encode: (val: Output) => Input;

  constructor({
    test,
    decode,
    encode,
  }: {
    test: (input: unknown) => input is Output,
    decode: (input: unknown, ctx: Context) => Maybe<Output>,
    encode: (val: Output) => Input,
  }) {
    this.test = test;
    this.decode = decode;
    this.encode = encode;
  }

  parse(input: unknown): Output {
    const ctx = new Context();
    const val = this.decode(input, ctx);
    if (val !== DECODE_ERROR) {
      return val;
    }
    // If the test failed, ctx.issue should be non-null. Adding a default value
    // in case it's missing.
    throw new ValidationError({
      schema: this,
      ...(ctx.issue ?? {path: ['$'], message: 'validation error'}),
    });
  }

  parseJson(input: string): Output {
    const data = JSON.parse(input);
    return this.parse(data);
  }

  stringifyJson(val: Output): string {
    return JSON.stringify(this.encode(val));
  }
}

/**
 * A helper to get the corresponding type from schema.
 * TODO(shik): Find a way to export it as z.infer<T> instead of Infer<T> without
 * using namespace or extra files.
 */
export type Infer<T> = T extends Schema<infer U, unknown>? U : never;
// `any` is used here since the first type parameter is invariant due to it
// being used in argument of `encode`.
// eslint-disable-next-line @typescript-eslint/no-explicit-any
type InferInput<T> = T extends Schema<any, infer U>? U : never;

function identity<T>(val: T): T {
  return val;
}

function createPrimitiveSchema<T>(
  test: (input: unknown) => input is T,
  error: string,
) {
  return new Schema<T>({
    test,
    decode(input, ctx) {
      if (test(input)) {
        return input;
      }
      ctx.setIssue(error);
      return DECODE_ERROR;
    },
    encode: identity,
  });
}

const nullSchema = createPrimitiveSchema(
  (input) => input === null,
  'expect null',
);

const booleanSchema = createPrimitiveSchema(
  (input) => typeof input === 'boolean',
  'expect boolean',
);

const numberSchema = createPrimitiveSchema(
  (input) => typeof input === 'number',
  'expect number',
);

const bigintSchema = createPrimitiveSchema(
  (input) => typeof input === 'bigint',
  'expect bigint',
);

const stringSchema = createPrimitiveSchema(
  (input) => typeof input === 'string',
  'expect string',
);

function createLiteralSchema<const T>(literal: T): Schema<T> {
  return createPrimitiveSchema<T>(
    (input): input is T => input === literal,
    `expect ${literal}`,
  );
}

type EnumObj = Record<string, number|string>;

function createNativeEnumSchema<T extends EnumObj>(
  enumObj: T,
): Schema<T[keyof T]> {
  return createPrimitiveSchema<T[keyof T]>(
    (input): input is T[keyof T] => checkEnumVariant(enumObj, input) !== null,
    'expect enum',
  );
}

function createOptionalSchema<T, I>(
  schema: Schema<T, I>,
): Schema<T|undefined, I|undefined> {
  return new Schema({
    // clang-format formats `T | null` to multiple lines, which is hard to
    // understand.
    // clang-format off
    test(input): input is T | undefined {
      if (input === undefined) {
        return true;
      }
      return schema.test(input);
    },
    // clang-format on
    decode(input, ctx) {
      if (input === undefined) {
        return undefined;
      }
      return schema.decode(input, ctx);
    },
    encode(val) {
      if (val === undefined) {
        return undefined;
      }
      return schema.encode(val);
    },
  });
}

function createNullableSchema<T, I>(
  schema: Schema<T, I>,
): Schema<T|null, I|null> {
  return new Schema({
    // clang-format formats `T | null` to multiple lines, which is hard to
    // understand.
    // clang-format off
    test(input): input is T | null {
      if (input === null) {
        return true;
      }
      return schema.test(input);
    },
    // clang-format on
    decode(input, ctx) {
      if (input === null) {
        return null;
      }
      return schema.decode(input, ctx);
    },
    encode(val) {
      if (val === null) {
        return null;
      }
      return schema.encode(val);
    },
  });
}

/**
 * Decodes undefined and null to null, and encodes null to null.
 *
 * This is useful to have backward compatible field, but still use `null` in
 * code.
 */
function createAutoNullOptionalSchema<T, I>(
  schema: Schema<T, I>,
): Schema<T|null, I|null|undefined> {
  return new Schema({
    // clang-format formats `T | null` to multiple lines, which is hard to
    // understand.
    // clang-format off
    test(input): input is T | null {
      if (input === null) {
        return true;
      }
      return schema.test(input);
    },
    // clang-format on
    decode(input, ctx) {
      if (input === null || input === undefined) {
        return null;
      }
      return schema.decode(input, ctx);
    },
    encode(val) {
      if (val === null) {
        return null;
      }
      return schema.encode(val);
    },
  });
}

function createCatchSchema<T, I>(
  schema: Schema<T, I>,
  fallback: T,
): Schema<T, I> {
  return new Schema({
    test: schema.test,
    decode(input, ctx) {
      const val = schema.decode(input, ctx);
      if (val === DECODE_ERROR) {
        return fallback;
      }
      return val;
    },
    encode(val) {
      return schema.encode(val);
    },
  });
}

function createArraySchema<T, I>(elem: Schema<T, I>): Schema<T[], I[]> {
  return new Schema({
    test(input): input is T[] {
      if (!Array.isArray(input)) {
        return false;
      }
      return input.every((v) => elem.test(v));
    },
    decode(input, ctx) {
      if (!Array.isArray(input)) {
        ctx.setIssue('expect array');
        return DECODE_ERROR;
      }

      const ret: T[] = [];
      for (let i = 0; i < input.length; i++) {
        ctx.pushKey(`${i}`);
        const val = elem.decode(input[i], ctx);
        if (val === DECODE_ERROR) {
          return DECODE_ERROR;
        }
        ret.push(val);
        ctx.popKey();
      }

      return ret;
    },
    encode(val) {
      return val.map((v) => elem.encode(v));
    },
  });
}

type SchemaArray = AnySchema[];

type InferTupleOutput<S> = S extends [infer Head, ...infer Tail] ?
  [Infer<Head>, ...InferTupleOutput<Tail>] :
  [];

type InferTupleInput<S> = S extends [infer Head, ...infer Tail] ?
  [InferInput<Head>, ...InferUnionInput<Tail>] :
  [];

function createTupleSchema<const T extends SchemaArray>(
  schemas: T,
): Schema<InferTupleOutput<T>, InferTupleInput<T>> {
  return new Schema({
    test(input): input is InferTupleOutput<T> {
      if (!Array.isArray(input) || input.length !== schemas.length) {
        return false;
      }
      return schemas.every((schema, i) => {
        const el = assertExists(input[i]);
        return schema.test(el);
      });
    },
    decode(input, ctx) {
      if (!Array.isArray(input) || input.length !== schemas.length) {
        ctx.setIssue(`expect tuple of length ${schemas.length}`);
        return DECODE_ERROR;
      }

      const ret: unknown[] = [];
      for (const [i, schema] of schemas.entries()) {
        ctx.pushKey(`${i}`);
        const el = assertExists(input[i]);
        const val = schema.decode(el, ctx);
        if (val === DECODE_ERROR) {
          return DECODE_ERROR;
        }
        ret.push(val);
        ctx.popKey();
      }

      // The entries in `ret` are from decoded values, so the type should be
      // correct.
      // eslint-disable-next-line @typescript-eslint/consistent-type-assertions
      return ret as InferTupleOutput<T>;
    },
    encode(val): InferTupleInput<T> {
      // The entries are from decoded values, so the type should be correct.
      // eslint-disable-next-line @typescript-eslint/consistent-type-assertions
      return schemas.map(
               (schema, i) => schema.encode(val[i]),
             ) as InferTupleInput<T>;
    },
  });
}

type SchemaObject = Record<string, AnySchema>;

// Used to expand the type from ObjectOutput<T> to make the type hint from
// language server more human-friendly.
type Expand<T> = T extends infer O ? {[K in keyof O]: O[K]} : never;

// Mark all fields that accept undefined as optional. Note that there is no
// explicit `undefined` in JSON, so it's technically correct for JSON.
type IfOptional<T, Y, N> = undefined extends T ? Y : N;

type InferObjectOutput<T> = {
  [K in keyof T as IfOptional<Infer<T[K]>, K, never>] +?: Infer<T[K]>;
}&{
  [K in keyof T as IfOptional<Infer<T[K]>, never, K>]: Infer<T[K]>;
};

type InferObjectInput<T> = {
  [K in keyof T as IfOptional<InferInput<T[K]>, K, never>] +?: InferInput<T[K]>;
}&{
  [K in keyof T as IfOptional<InferInput<T[K]>, never, K>]: InferInput<T[K]>;
};

type InferObjectSchema<T> =
  Schema<Expand<InferObjectOutput<T>>, Expand<InferObjectInput<T>>>;

function createObjectSchema<T extends SchemaObject>(
  spec: T,
): InferObjectSchema<T> {
  return new Schema({
    test(input): input is Expand<InferObjectOutput<T>> {
      if (typeof input !== 'object' || input === null) {
        return false;
      }
      for (const [key, schema] of Object.entries(spec)) {
        // We're deliberately casting to access arbitrary key of the object.
        /* eslint-disable @typescript-eslint/consistent-type-assertions */
        const value = Object.hasOwn(input, key) ?
          (input as Record<string, unknown>)[key] :
          undefined;
        /* eslint-enable @typescript-eslint/consistent-type-assertions */
        if (!schema.test(value)) {
          return false;
        }
      }
      return true;
    },
    decode(input, ctx): Maybe<Expand<InferObjectOutput<T>>> {
      if (typeof input !== 'object') {
        ctx.setIssue('expect object');
        return DECODE_ERROR;
      }

      if (input === null) {
        ctx.setIssue('expect non-null object');
        return DECODE_ERROR;
      }

      const obj: Record<string, unknown> = {};
      for (const [key, schema] of Object.entries(spec)) {
        ctx.pushKey(key);
        // We're deliberately casting to access arbitrary key of the object.
        /* eslint-disable @typescript-eslint/consistent-type-assertions */
        const value = Object.hasOwn(input, key) ?
          (input as Record<string, unknown>)[key] :
          undefined;
        /* eslint-enable @typescript-eslint/consistent-type-assertions */
        const decodedValue = schema.decode(value, ctx);
        if (decodedValue === DECODE_ERROR) {
          return DECODE_ERROR;
        }
        obj[key] = decodedValue;
        ctx.popKey();
      }

      // The keys and values are derived from the spec, so the `obj` should
      // always follow the type.
      // eslint-disable-next-line @typescript-eslint/consistent-type-assertions
      return obj as Expand<InferObjectOutput<T>>;
    },
    encode(val) {
      const ret: Record<string, unknown> = {};
      for (const [key, schema] of Object.entries(spec)) {
        // We're deliberately casting to access arbitrary key of the object.
        /* eslint-disable @typescript-eslint/consistent-type-assertions */
        const value = Object.hasOwn(val, key) ?
          (val as Record<string, unknown>)[key] :
          undefined;
        /* eslint-enable @typescript-eslint/consistent-type-assertions */
        ret[key] = schema.encode(value);
      }

      // The keys and values are derived from the spec, so the `obj` should
      // always follow the type.
      // eslint-disable-next-line @typescript-eslint/consistent-type-assertions
      return ret as Expand<InferObjectInput<T>>;
    },
  });
}

type InferUnionOutput<S> = S extends [infer Head, ...infer Tail] ?
  Infer<Head>|InferUnionOutput<Tail>:
  never;

type InferUnionInput<S> = S extends [infer Head, ...infer Tail] ?
  InferInput<Head>|InferUnionInput<Tail>:
  never;

function createUnionSchema<const T extends SchemaArray>(
  schemas: T,
): Schema<InferUnionOutput<T>, InferUnionInput<T>> {
  return new Schema({
    test(input): input is InferUnionOutput<T> {
      // TODO(shik): Expose the issues found in alternatives.
      return schemas.some((s) => s.test(input));
    },
    decode(input, ctx): Maybe<InferUnionOutput<T>> {
      // TODO(shik): Expose the issues found in alternatives.
      const altCtx = new Context();
      for (const schema of schemas) {
        const val = schema.decode(input, altCtx);
        if (val !== DECODE_ERROR) {
          // One of the alternative decodes the input correctly, so the returned
          // value type would be the alternative.
          /* eslint-disable @typescript-eslint/consistent-type-assertions */
          return val as InferUnionOutput<T>;
          /* eslint-enable @typescript-eslint/consistent-type-assertions */
        }
      }
      ctx.setIssue('expect union but all alternatives failed');
      return DECODE_ERROR;
    },
    encode(val): InferUnionInput<T> {
      for (const schema of schemas) {
        if (schema.test(val)) {
          // One of the alternative validates the value correctly, so it should
          // be able to encode the value.
          /* eslint-disable @typescript-eslint/consistent-type-assertions */
          return schema.encode(val) as InferUnionInput<T>;
          /* eslint-enable @typescript-eslint/consistent-type-assertions */
        }
      }
      assertNotReached(
        'union schema encode value with no alternatives matched',
      );
    },
  });
}

type InferIntersectionOutput<S> = S extends [infer Head, ...infer Tail] ?
  Infer<Head>&InferIntersectionOutput<Tail>:
  unknown;

type InferIntersectionInput<S> = S extends [infer Head, ...infer Tail] ?
  InferInput<Head>&InferIntersectionInput<Tail>:
  unknown;

// Note that intersection only supports intersection of several record types,
// and only does a shallow merge of the decoded value, since it's the most
// useful case and it's a bit clearer what encode should do in this case.
function createIntersectionSchema<const T extends SchemaArray>(
  schemas: T,
):
  Schema<
    Expand<InferIntersectionOutput<T>>, Expand<InferIntersectionInput<T>>> {
  return new Schema({
    test(input): input is Expand<InferIntersectionOutput<T>> {
      return !schemas.some((s) => !s.test(input));
    },
    decode(input, ctx): Maybe<Expand<InferIntersectionOutput<T>>> {
      const obj: Record<string, unknown> = {};
      for (const schema of schemas) {
        const decodedValue = schema.decode(input, ctx);
        if (decodedValue === DECODE_ERROR) {
          return DECODE_ERROR;
        }
        if (typeof decodedValue !== 'object' || decodedValue === null) {
          ctx.setIssue('decoded value in intersection is not object');
          return DECODE_ERROR;
        }
        for (const [key, val] of Object.entries(decodedValue)) {
          if (Object.hasOwn(obj, key)) {
            ctx.setIssue('duplicate key in intersection alternatives');
            return DECODE_ERROR;
          }
          obj[key] = val;
        }
      }

      // The values are derived from the spec, so the `obj` should always
      // follow the type.
      // eslint-disable-next-line @typescript-eslint/consistent-type-assertions
      return obj as Expand<InferIntersectionOutput<T>>;
    },
    encode(val): Expand<InferIntersectionInput<T>> {
      const obj: Record<string, unknown> = {};
      for (const schema of schemas) {
        const encodedValue = schema.encode(val);
        if (typeof encodedValue !== 'object' || encodedValue === null) {
          throw new Error('encoded value in intersection is not object');
        }
        for (const [key, val] of Object.entries(encodedValue)) {
          if (Object.hasOwn(obj, key)) {
            throw new Error('duplicate key in intersection alternatives');
          }
          obj[key] = val;
        }
      }

      // The values are derived from the spec, so the `obj` should always
      // follow the type.
      // eslint-disable-next-line @typescript-eslint/consistent-type-assertions
      return obj as Expand<InferIntersectionInput<T>>;
    },
  });
}

function createTransformSchema<T, I, NewT>(
  schema: Schema<T, I>,
  {
    test,
    decode,
    encode,
  }: {
    test: (input: unknown) => input is NewT,
    decode: (val: T) => NewT,
    encode: (val: NewT) => T,
  },
): Schema<NewT, I> {
  return new Schema({
    test,
    decode(input, ctx) {
      const val = schema.decode(input, ctx);
      if (val === DECODE_ERROR) {
        return DECODE_ERROR;
      }
      return decode(val);
    },
    encode(val) {
      return schema.encode(encode(val));
    },
  });
}

function createWithDefaultSchema<T, I>(
  schema: Schema<T, I>,
  defaultValue: T,
): Schema<T, I|undefined> {
  return new Schema({
    test: schema.test,
    decode(val, ctx) {
      if (val === undefined) {
        return defaultValue;
      }
      return schema.decode(val, ctx);
    },
    encode(val) {
      return schema.encode(val);
    },
  });
}

/**
 * A minimal Zod-like interface.
 */
export const z = {
  'null': (): Schema<null> => nullSchema,
  'boolean': (): Schema<boolean> => booleanSchema,
  'number': (): Schema<number> => numberSchema,
  'bigint': (): Schema<bigint> => bigintSchema,
  'string': (): Schema<string> => stringSchema,
  'literal': createLiteralSchema,
  'nativeEnum': createNativeEnumSchema,
  'optional': createOptionalSchema,
  'nullable': createNullableSchema,
  'autoNullOptional': createAutoNullOptionalSchema,
  'catch': createCatchSchema,
  'array': createArraySchema,
  'tuple': createTupleSchema,
  'object': createObjectSchema,
  'union': createUnionSchema,
  'intersection': createIntersectionSchema,
  'transform': createTransformSchema,
  'withDefault': createWithDefaultSchema,
};
