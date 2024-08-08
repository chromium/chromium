// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Verify |condition| is truthy and return |condition| if so.
 *
 * @param condition A condition to check for truthiness.  Note that this
 *     may be used to test whether a value is defined or not, and we don't want
 *     to force a cast to Boolean.
 * @param optMessage A message to show on failure.
 */
export function assert(
  condition: boolean,
  optMessage?: string,
): asserts condition {
  if (!condition) {
    let message = 'Assertion failed';
    if (optMessage !== undefined) {
      message = message + ': ' + optMessage;
    }
    throw new Error(message);
  }
}

/**
 * Call this from places in the code that should never be reached.
 *
 * This code should only be hit in the case of serious programmer error.
 *
 * @param optMessage A message to show when this is hit.
 */
export function assertNotReached(optMessage = 'Unreachable code hit'): never {
  assert(false, optMessage);
}

/**
 * Throw an exception on unexpected values.
 *
 * This can be used along with type narrowing to ensure at compile time that all
 * possible types for a value have been handled.
 *
 * A common use-case is in switch statements:
 *
 * ```
 * // enumValue: Enum.A | Enum.B
 * switch(enumValue) {
 *   case Enum.A:
 *     break;
 *   case Enum.B:
 *     break;
 *   default:
 *     assertExhaustive(enumValue);
 *     break;
 * }
 * ```.
 *
 * @param value The value to be checked.
 * @param optMessage An optional error message to throw.
 */
export function assertExhaustive(
  value: never,
  optMessage = `unexpected value ${value}`,
): never {
  assert(false, optMessage);
}

type EnumObj = Record<string, number|string>;

/**
 * Check if a string or number value is a variant of an enum.
 *
 * @param enumType The enum type to be checked.
 * @param value Value to be checked.
 * @return The value if it's an enum variant, null otherwise.
 */
export function checkEnumVariant<T extends EnumObj>(
  enumType: T,
  value: unknown,
): T[keyof T]|null {
  // `includes` work for any value types.
  // eslint-disable-next-line @typescript-eslint/consistent-type-assertions
  if (!Object.values(enumType).includes(value as T[keyof T])) {
    return null;
  }

  // We just checked that the value is a valid enum variant, so the cast is
  // safe.
  // eslint-disable-next-line @typescript-eslint/consistent-type-assertions
  return value as T[keyof T];
}

/**
 * Asserts that a string or number value is a variant of an enum.
 *
 * @param enumType The enum type to be checked.
 * @param value Value to be checked.
 * @return The value if it's an enum variant, throws assertion error otherwise.
 */
export function assertEnumVariant<T extends EnumObj>(
  enumType: T,
  value: unknown,
): T[keyof T] {
  const ret = checkEnumVariant(enumType, value);
  assert(ret !== null, `${value} is not a valid enum variant`);
  return ret;
}

// "unknown" doesn't work well here if the constructor have overloads with
// different numbers of argument and strictNullChecks on.
// eslint-disable-next-line @typescript-eslint/no-explicit-any
type Constructor<T> = new (...args: any[]) => T;

/**
 * Check if a value is an instance of a class.
 *
 * @param value The value to check.
 * @param ctor A user-defined constructor.
 */
export function checkInstanceof<T>(
  value: unknown,
  ctor: Constructor<T>,
): T|null {
  if (!(value instanceof ctor)) {
    return null;
  }
  return value;
}

/**
 * Assert a value is an instance of a class.
 *
 * @param value The value to check.
 * @param ctor A user-defined constructor.
 * @param optMessage A message to show when this is hit.
 */
export function assertInstanceof<T>(
  value: unknown,
  ctor: Constructor<T>,
  optMessage?: string,
): T {
  // We don't use assert immediately here so that we avoid constructing an error
  // message if we don't have to.
  if (!(value instanceof ctor)) {
    assert(
      false,
      optMessage ?? `Value ${value} is not a[n] ${ctor.name ?? typeof ctor}`,
    );
  }
  return value;
}

/**
 * Assert a value is not null or undefined.
 *
 * @param value The value to check.
 * @param optMessage A message to show when this is hit.
 */
export function assertExists<T>(
  value: T|null|undefined,
  optMessage?: string,
): T {
  if (value === null || value === undefined) {
    assert(false, optMessage ?? `Value is ${value}`);
  }
  return value;
}

/**
 * Assert a value is a string.
 *
 * @param value The value to check.
 * @param optMessage A message to show when this is hit.
 */
export function assertString(value: unknown, optMessage?: string): string {
  assert(
    typeof value === 'string',
    optMessage ?? `Value ${value} is not a string`,
  );
  return value;
}
