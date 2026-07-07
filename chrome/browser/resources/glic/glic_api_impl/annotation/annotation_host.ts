// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {AnnotationHandlerInterface, ScrollToSelector as ScrollToSelectorMojo} from '../../glic.mojom-webui.js';
import type {ScrollToParams} from '../../glic_api/glic_api.js';
import {ScrollToErrorReason} from '../../glic_api/glic_api.js';
import {enumToClient} from '../enum_conversions.js';
import {urlFromClient} from '../host/conversions.js';
import {ErrorWithReasonImpl} from '../request_types.js';
import type {MessageHandlerInterface} from '../transport/messaging.js';

import type {AnnotationHost} from './annotation_types.js';

export class AnnotationHostMessageHandler implements
    MessageHandlerInterface<AnnotationHost> {
  constructor(private annotationHandler: AnnotationHandlerInterface) {}

  async scrollTo(request: {params: ScrollToParams}): Promise<void> {
    const {params} = request;

    function getMojoSelector(): ScrollToSelectorMojo {
      const {selector} = params;
      if (selector.exactText !== undefined) {
        if (selector.exactText.searchRangeStartNodeId !== undefined &&
            params.documentId === undefined) {
          throw new ErrorWithReasonImpl(
              'scrollTo', ScrollToErrorReason.NOT_SUPPORTED,
              'searchRangeStartNodeId without documentId');
        }
        return {
          exactText: {
            text: selector.exactText.text,
            searchRangeStartNodeId:
                selector.exactText.searchRangeStartNodeId ?? null,
          },
        };
      }
      if (selector.textFragment !== undefined) {
        if (selector.textFragment.searchRangeStartNodeId !== undefined &&
            params.documentId === undefined) {
          throw new ErrorWithReasonImpl(
              'scrollTo', ScrollToErrorReason.NOT_SUPPORTED,
              'searchRangeStartNodeId without documentId');
        }
        return {
          textFragment: {
            textStart: selector.textFragment.textStart,
            textEnd: selector.textFragment.textEnd,
            searchRangeStartNodeId:
                selector.textFragment.searchRangeStartNodeId ?? null,
          },
        };
      }
      if (selector.node !== undefined) {
        if (params.documentId === undefined) {
          throw new ErrorWithReasonImpl(
              'scrollTo', ScrollToErrorReason.NOT_SUPPORTED,
              'nodeId without documentId');
        }
        return {
          node: {
            nodeId: selector.node.nodeId,
          },
        };
      }
      throw new ErrorWithReasonImpl(
          'scrollTo', ScrollToErrorReason.NOT_SUPPORTED);
    }

    const mojoParams = {
      highlight: params.highlight === undefined ? true : params.highlight,
      selector: getMojoSelector(),
      documentId: params.documentId ?? null,
      url: params.url ? urlFromClient(params.url) : null,
    };
    const {errorReason} = (await this.annotationHandler.scrollTo(mojoParams));
    if (errorReason !== null) {
      throw new ErrorWithReasonImpl('scrollTo', enumToClient(errorReason));
    }
    return;
  }

  dropScrollToHighlight(): void {
    this.annotationHandler.dropScrollToHighlight();
  }
}
