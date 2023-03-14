// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Asserts that the given type is never.
 */
export type AssertNever<T extends never> = T;

type CheckEnumValuesOverlapImpl<Union, Overlap, Enums> =
    Enums extends [infer Enum extends string, ...infer Rest] ?
    CheckEnumValuesOverlapImpl<
        Union|`${Enum}`, Overlap|(Union&`${Enum}`), Rest>:
    Overlap;

/**
 * Checks that a tuple of enum have pairwise non-overlapping values.
 *
 * The result type will be |never| if there's no overlap, and union of the
 * overlapped values if there's overlap.
 */
export type CheckEnumValuesOverlap<Enums extends string[]> =
    CheckEnumValuesOverlapImpl<never, never, Enums>;
