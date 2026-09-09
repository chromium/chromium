// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from '//resources/js/assert.js';

import {enumToClient} from '../../enum_conversions.js';
import {AnnotationHandlerRemote} from '../../glic.mojom-webui.js';
import type {ScrollToSelector as ScrollToSelectorMojo, WebClientHandlerRemote, WebClientInitialState} from '../../glic.mojom-webui.js';
import type {GlicBrowserHost, ScrollToParams} from '../../glic_api/glic_api.js';
import {ScrollToErrorReason} from '../../glic_api/glic_api.js';
import {urlFromClient} from '../host/conversions.js';
import {maybeWrapWithLogging} from '../mojo_logging.js';
import {ErrorWithReasonImpl} from '../request_types.js';

export class GlicBrowserHostAnnotation implements Partial<GlicBrowserHost> {
  private annotationHandler?: AnnotationHandlerRemote;
  private handler?: WebClientHandlerRemote;

  initialize(
      initialState: WebClientInitialState, handler: WebClientHandlerRemote) {
    if (!initialState.enableScrollTo || !handler) {
      this.scrollTo = undefined;
      this.dropScrollToHighlight = undefined;
      return;
    }

    this.handler = handler;
  }

  destroyAnnotation(): void {
    if (this.annotationHandler) {
      this.annotationHandler.$.close();
      this.annotationHandler = undefined;
    }
    this.handler = undefined;
    this.scrollTo = undefined;
    this.dropScrollToHighlight = undefined;
  }

  async scrollTo?(params: ScrollToParams): Promise<void> {
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
    const {errorReason} =
        await this.getAnnotationHandler().scrollTo(mojoParams);
    if (errorReason !== null) {
      throw new ErrorWithReasonImpl('scrollTo', enumToClient(errorReason));
    }
  }

  dropScrollToHighlight?(): void {
    this.getAnnotationHandler().dropScrollToHighlight();
  }

  private getAnnotationHandler() {
    if (this.annotationHandler) {
      return this.annotationHandler;
    }
    assert(this.handler);
    this.annotationHandler = maybeWrapWithLogging(
        new AnnotationHandlerRemote(), {prefix: 'AnnotationHandler'});
    this.handler.createAnnotationHandler(
        this.annotationHandler.$.bindNewPipeAndPassReceiver());
    return this.annotationHandler;
  }
}
