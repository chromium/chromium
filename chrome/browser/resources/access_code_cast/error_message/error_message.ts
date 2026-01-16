// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {I18nMixinLit} from '//resources/cr_elements/i18n_mixin_lit.js';
import {assertNotReachedCase} from '//resources/js/assert.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import {AddSinkResultCode} from '../access_code_cast.mojom-webui.js';
import {RouteRequestResultCode} from '../route_request_result_code.mojom-webui.js';

import {getCss} from './error_message.css.js';
import {getHtml} from './error_message.html.js';

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

const ErrorMessageElementBase = I18nMixinLit(CrLitElement);

export class ErrorMessageElement extends ErrorMessageElementBase {
  static get is() {
    return 'c2c-error-message';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      messageCode: {type: Number},
    };
  }

  protected accessor messageCode: ErrorMessage = ErrorMessage.NO_ERROR;

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

  protected getErrorMessage(): string {
    switch (this.messageCode) {
      case ErrorMessage.NO_ERROR:
        return '';
      case ErrorMessage.GENERIC:
        return this.i18n('errorUnknown');
      case ErrorMessage.ACCESS_CODE:
        return this.i18n('errorAccessCode');
      case ErrorMessage.NETWORK:
        return this.i18n('errorNetwork');
      case ErrorMessage.PERMISSION:
        return this.i18n('errorPermission');
      case ErrorMessage.TOO_MANY_REQUESTS:
        return this.i18n('errorTooManyRequests');
      case ErrorMessage.PROFILE_SYNC_ERROR:
        return this.i18n('errorProfileSync');
      case ErrorMessage.DIFFERENT_NETWORK:
        return this.i18n('errorDifferentNetwork');
      default:
        assertNotReachedCase(this.messageCode);
    }
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
