// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type * as actorWebUiMojom from '../actor_webui.mojom-webui.js';
import type * as mojom from '../glic.mojom-webui.js';
import type * as api from '../glic_api/glic_api.js';

import type * as requestTypes from './request_types.js';


/* eslint-disable-next-line @typescript-eslint/naming-convention */
function assertNever<_T extends never>() {}

// Helper function to shallow-copy an object and replace some properties.
// Useful to convert from these private types to public types. This will fail to
// compile if a property is missed.
export function replaceProperties<O, R>(
    original: O, replacements: R): Omit<O, keyof R>&R {
  return Object.assign(Object.assign({}, original) as any, replacements);
}



//
// This code checks that mojom enums are equivalent to their counterparts in
// the API.
//

type CamelCaseToSnakeCase<S extends string> =
    S extends `${infer First}${infer Rest}` ?
    First extends Lowercase<First>?
    `${First}${CamelCaseToSnakeCase < Rest >}` :
    `${
        CamelCaseToSnakeCase<Rest> extends '' ?
            Uppercase<First>:
            `_${Uppercase < First >}${CamelCaseToSnakeCase < Rest >}`}` :
    S;

type RemoveFirstK<S extends string> = S extends `k${infer Rest}` ? Rest : S;

type RemoveFirstUnderscore<S extends string> =
    S extends `_${infer Rest}` ? Rest : S;

type ConvertKey<T extends string> =
    RemoveFirstUnderscore<Uppercase<CamelCaseToSnakeCase<RemoveFirstK<T>>>>;

type AllMojomEnumKeysAsUppercase<E> = keyof {
  [K in Exclude<keyof E, 'MIN_VALUE'|'MAX_VALUE'>as
       K extends string ? ConvertKey<K>: never]: void;
};

type AllEnumKeys<E> = keyof {
  [K in keyof E as K extends string ? K : never]: void;
};

type AnnotateError<T, M> = T extends never ? never : [M, T];

// Checks that both enums share the same keys.
// Returns never on success, or an error message otherwise.
type CheckEnumCompatibility<MojoEnum, TsEnum> = AnnotateError<
    Exclude<AllMojomEnumKeysAsUppercase<MojoEnum>, AllEnumKeys<TsEnum>>,
    'typescript enum missing value'>|
    AnnotateError<
        Exclude<AllEnumKeys<TsEnum>, AllMojomEnumKeysAsUppercase<MojoEnum>>,
        'mojo enum missing value'>;

//
// Any enums shared between mojo and glic_api should be checked here. Please
// add a check if you add a new enum.
//

// Ignore FLOATING and DOCKED in the api, as they're just deprecated aliases.
assertNever<CheckEnumCompatibility<
    typeof mojom.PanelStateKind,
    Omit<typeof api.PanelStateKind, 'FLOATING'|'DOCKED'>>>();
// kUnknown isn't in the public API because this is a closed enum, and will not
// be expanded.
assertNever<CheckEnumCompatibility<
    Omit<typeof mojom.WebClientMode, 'kUnknown'>, typeof api.WebClientMode>>();
assertNever<CheckEnumCompatibility<
    typeof mojom.CaptureScreenshotErrorReason,
    typeof api.CaptureScreenshotErrorReason>>();
assertNever<CheckEnumCompatibility<
    typeof mojom.ScrollToErrorReason, typeof api.ScrollToErrorReason>>();
assertNever<CheckEnumCompatibility<
    typeof mojom.InvocationSource, typeof api.InvocationSource>>();
assertNever<CheckEnumCompatibility<
    Omit<typeof mojom.SettingsPageField, 'kNone'>,
    typeof api.SettingsPageField>>();
assertNever<CheckEnumCompatibility<
    typeof mojom.HostCapability, typeof api.HostCapability>>();
assertNever<CheckEnumCompatibility<
    typeof mojom.ActorTaskState, typeof api.ActorTaskState>>();
assertNever<CheckEnumCompatibility<
    typeof mojom.ActorTaskPauseReason, typeof api.ActorTaskPauseReason>>();
assertNever<CheckEnumCompatibility<
    typeof mojom.ActorTaskStopReason, typeof api.ActorTaskStopReason>>();
assertNever<CheckEnumCompatibility<
    typeof actorWebUiMojom.UserGrantedPermissionDuration,
    typeof api.UserGrantedPermissionDuration>>();
assertNever<CheckEnumCompatibility<
    typeof actorWebUiMojom.SelectCredentialDialogErrorReason,
    typeof requestTypes.SelectCredentialDialogErrorReason>>();
assertNever<CheckEnumCompatibility<
    typeof actorWebUiMojom.ConfirmationRequestErrorReason,
    typeof requestTypes.ConfirmationRequestErrorReason>>();
assertNever<CheckEnumCompatibility<
    typeof mojom.MetricUserInputReactionType,
    typeof api.MetricUserInputReactionType>>();
