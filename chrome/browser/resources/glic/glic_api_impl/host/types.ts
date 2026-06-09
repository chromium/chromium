// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Defines some types shared across files in host/.

import {ScrollToErrorReason} from '../../glic_api/glic_api.js';
import type {HostRequestTypes} from '../../glic_api_impl/request_types.js';
import {ErrorWithReasonImpl} from '../../glic_api_impl/request_types.js';
import type {ResponsePayload} from '../transport/messaging.js';

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
      HostBackgroundResponse<ResponsePayload<HostRequestTypes, RequestName>>;
};

// How to respond to each requests received in the background. One entry for
// each request type that does not specify `backgroundAllowed`.
export const BACKGROUND_RESPONSES: HostBackgroundResponseMap = {
  createTab: {returns: {}},
  showProfilePicker: {throws: true},
  getContextFromFocusedTab: {throws: true},
  getContextFromTab: {throws: true},
  captureScreenshot: {throws: true},
  scrollTo: {
    does: () => {
      throw new ErrorWithReasonImpl(
          'scrollTo', ScrollToErrorReason.NOT_SUPPORTED);
    },
  },
  openOsPermissionSettingsMenu: {throws: true},
  pinTabs: {returns: {pinnedAll: false}},
  unpinAllTabs: {returns: undefined},
  createSkill: {returns: {modalOpened: false}},
  updateSkill: {returns: {modalOpened: false}},
  getSkill: {returns: {}},
  subscribeToPinCandidates: {returns: undefined},
  getZeroStateSuggestionsForFocusedTab: {returns: {}},
  getZeroStateSuggestionsAndSubscribe: {returns: {}},
};

export enum PanelOpenState {
  OPEN,
  CLOSED,
}
