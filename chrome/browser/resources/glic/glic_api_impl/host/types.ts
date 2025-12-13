// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Defines some types shared across files in host/.

import {ScrollToErrorReason} from '../../glic_api/glic_api.js';
import type {HostRequestTypes, RequestResponseType} from '../../glic_api_impl/request_types.js';
import {ErrorWithReasonImpl} from '../../glic_api_impl/request_types.js';

// Is a request type allowed in the background?
type IsBackgroundRequest<T extends keyof HostRequestTypes> =
    'backgroundAllowed' extends keyof HostRequestTypes[T] ?
    HostRequestTypes[T]['backgroundAllowed'] :
    false;

// Configuration of how to handle requests received while glic is in the
// background. Requests that are annotated with `backgroundAllowed` are
// unaffected. Otherwise, an entry must exist in `BACKGROUND_RESPONSES`
// to define behavior.
// Note that if glic becomes backgrounded while a request is being processed,
// the request will not be affected.

// Throw an exception, returning an error to the client.
interface HostBackgroundResponseThrows {
  throws: true;
}

// Run `does()` and return its result to the client.
export interface HostBackgroundResponseDoes<R> {
  does: () => R;
}

// Returns a constant value to the client.
export interface HostBackgroundResponseReturns<R> {
  returns: R;
}

export type HostBackgroundResponse<R> = HostBackgroundResponseThrows|
    HostBackgroundResponseReturns<R>|HostBackgroundResponseDoes<R>;

type HostBackgroundResponseMap = {
  [RequestName in keyof HostRequestTypes as
       IsBackgroundRequest<RequestName> extends true ? never : RequestName]:
      HostBackgroundResponse<RequestResponseType<RequestName>>;
};

// How to respond to each requests received in the background. One entry for
// each request type that does not specify `backgroundAllowed`.
export const BACKGROUND_RESPONSES: HostBackgroundResponseMap = {
  glicBrowserCreateTab: {returns: {}},
  glicBrowserShowProfilePicker: {throws: true},
  glicBrowserGetContextFromFocusedTab: {throws: true},
  glicBrowserGetContextFromTab: {throws: true},
  glicBrowserCaptureScreenshot: {throws: true},
  glicBrowserScrollTo: {
    does: () => {
      throw new ErrorWithReasonImpl(
          'scrollTo', ScrollToErrorReason.NOT_SUPPORTED);
    },
  },
  glicBrowserOpenOsPermissionSettingsMenu: {throws: true},
  glicBrowserPinTabs: {returns: {pinnedAll: false}},
  glicBrowserUnpinAllTabs: {returns: undefined},
  glicBrowserSubscribeToPinCandidates: {returns: undefined},
  glicBrowserGetZeroStateSuggestionsForFocusedTab: {returns: {}},
  glicBrowserGetZeroStateSuggestionsAndSubscribe: {returns: {}},
};

export enum PanelOpenState {
  OPEN,
  CLOSED,
}
