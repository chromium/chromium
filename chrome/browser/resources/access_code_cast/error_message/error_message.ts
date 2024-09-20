// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/icons_lit.html.js';
import 'chrome://resources/cr_elements/cr_shared_vars.css.js';

import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {AddSinkResultCode} from '../access_code_cast.mojom-webui.js';
import {RouteRequestResultCode} from '../route_request_result_code.mojom-webui.js';

import {getTemplate} from './error_message.html.js';

enum ErrorMessage {
  NO_ERROR,
  GENERIC,
  ACCESS_CODE,
  NETWORK,
  PERMISSION,
  TOO_MANY_REQUESTS,
  PROFILE_SYNC_ERROR,
  DIFFERENT_NETWORK
}

export class ErrorMessageElement extends PolymerElement {
  private static readonly ADD_RESULT_MESSAGE_CODES:
      Array<[ErrorMessage, AddSinkResultCode[]]> = [
        [ErrorMessage.NO_ERROR, [AddSinkResultCode.OK]],
        [
          ErrorMessage.GENERIC,
          [
            AddSinkResultCode.UNKNOWN_ERROR,
            AddSinkResultCode.SINK_CREATION_ERROR,
            AddSinkResultCode.INTERNAL_MEDIA_ROUTER_ERROR,
          ],
        ],
        [
          ErrorMessage.ACCESS_CODE,
          [
            AddSinkResultCode.INVALID_ACCESS_CODE,
            AddSinkResultCode.ACCESS_CODE_NOT_FOUND,
          ],
        ],
        [
          ErrorMessage.NETWORK,
          [
            AddSinkResultCode.HTTP_RESPONSE_CODE_ERROR,
            AddSinkResultCode.RESPONSE_MALFORMED,
            AddSinkResultCode.EMPTY_RESPONSE,
            AddSinkResultCode.SERVICE_NOT_PRESENT,
            AddSinkResultCode.SERVER_ERROR,
          ],
        ],
        [ErrorMessage.PERMISSION, [AddSinkResultCode.AUTH_ERROR]],
        [ErrorMessage.TOO_MANY_REQUESTS, [AddSinkResultCode.TOO_MANY_REQUESTS]],
        [
          ErrorMessage.PROFILE_SYNC_ERROR,
          [AddSinkResultCode.PROFILE_SYNC_ERROR],
        ],
        [
          ErrorMessage.DIFFERENT_NETWORK,
          [AddSinkResultCode.CHANNEL_OPEN_ERROR],
        ],
      ];

  private static readonly CAST_RESULT_MESSAGE_CODES:
      Array<[ErrorMessage, RouteRequestResultCode[]]> = [
        [ErrorMessage.NO_ERROR, [RouteRequestResultCode.OK]],
        [
          ErrorMessage.GENERIC,
          [
            RouteRequestResultCode.UNKNOWN_ERROR,
            RouteRequestResultCode.INVALID_ORIGIN,
            RouteRequestResultCode.DEPRECATED_OFF_THE_RECORD_MISMATCH,
            RouteRequestResultCode.NO_SUPPORTED_PROVIDER,
            RouteRequestResultCode.CANCELLED,
            RouteRequestResultCode.ROUTE_ALREADY_EXISTS,
            RouteRequestResultCode.DESKTOP_PICKER_FAILED,
            RouteRequestResultCode.ROUTE_ALREADY_TERMINATED,
          ],
        ],
        [
          ErrorMessage.NETWORK,
          [
            RouteRequestResultCode.TIMED_OUT,
            RouteRequestResultCode.ROUTE_NOT_FOUND,
            RouteRequestResultCode.SINK_NOT_FOUND,
          ],
        ],
      ];

  private static readonly ADD_RESULT_MESSAGE_MAP =
      new Map(ErrorMessageElement.ADD_RESULT_MESSAGE_CODES);

  private static readonly CAST_RESULT_MESSAGE_MAP =
      new Map(ErrorMessageElement.CAST_RESULT_MESSAGE_CODES);

  // Needed for Polymer data binding
  private errorMessageEnum = ErrorMessage;

  static get is() {
    return 'c2c-error-message';
  }

  static get template() {
    return getTemplate();
  }

  private messageCode = ErrorMessage.NO_ERROR;

  setAddSinkError(resultCode: AddSinkResultCode) {
    this.messageCode = this.findErrorMessage(resultCode,
      ErrorMessageElement.ADD_RESULT_MESSAGE_MAP);
  }

  setCastError(resultCode: RouteRequestResultCode) {
    this.messageCode = this.findErrorMessage(resultCode,
      ErrorMessageElement.CAST_RESULT_MESSAGE_MAP);
  }

  setNoError() {
    this.messageCode = ErrorMessage.NO_ERROR;
  }

  getMessageCode() {
    return this.messageCode;
  }

  isEqual(a: ErrorMessage, b: ErrorMessage) {
    return a === b;
  }

  isNotEqual(a: ErrorMessage, b: ErrorMessage) {
    return a !== b;
  }

  private findErrorMessage(
      resultCode: AddSinkResultCode|RouteRequestResultCode,
      messageCodes: Map<ErrorMessage, any[]>) {
    for (const key of messageCodes.keys()) {
      if (messageCodes.get(key)!.includes(resultCode)) {
        return key;
      }
    }

    return ErrorMessage.NO_ERROR;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'c2c-error-message': ErrorMessageElement;
  }
}

customElements.define(ErrorMessageElement.is, ErrorMessageElement);
