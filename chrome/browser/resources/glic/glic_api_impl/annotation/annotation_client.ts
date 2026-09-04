// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {WebClientInitialState} from '../../glic.mojom-webui.js';
import type {GlicBrowserHost, ScrollToParams} from '../../glic_api/glic_api.js';
import type {GlicBrowserHostBaseContext} from '../client/glic_client_common.js';
import type {PendingReceiver, PostMessageRemote} from '../transport/post_message_transport.js';

import {AnnotationHostDef} from './annotation_types.js';
import type {AnnotationHost} from './annotation_types.js';

export class GlicBrowserHostAnnotation implements Partial<GlicBrowserHost> {
  private annotationSender?: PostMessageRemote<AnnotationHost>;
  private annotationReceiver?: PendingReceiver<AnnotationHost>;

  constructor(private host: GlicBrowserHostBaseContext) {}

  initialize(initialState: WebClientInitialState) {
    if (!initialState.enableScrollTo) {
      this.scrollTo = undefined;
      this.dropScrollToHighlight = undefined;
      return;
    }

    const {remote, receiver} =
        this.host.router.newPipeWithRemote(AnnotationHostDef);
    this.annotationSender = remote;
    this.annotationReceiver = receiver;
  }

  async scrollTo?(params: ScrollToParams): Promise<void> {
    this.ensureAnnotationHandlerCreated();
    return this.annotationSender!.requestWithResponse('scrollTo', {params});
  }

  dropScrollToHighlight?(): void {
    this.ensureAnnotationHandlerCreated();
    this.annotationSender!.requestNoResponse(
        'dropScrollToHighlight', undefined);
  }

  private ensureAnnotationHandlerCreated() {
    if (this.annotationReceiver === undefined) {
      return;
    }
    this.host.clientRemote.requestNoResponse('createAnnotationHandler', {
      annotationReceiver: this.annotationReceiver,
    });
    this.annotationReceiver = undefined;
  }
}
