// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert, assertExists} from './assert.js';
import * as Comlink from './lib/comlink.js';
import {isLocalDev} from './models/load_time_data.js';
import {
  Ga4EventParams,
  Ga4MetricDimension,
  GaBaseEvent,
  GaHelper,
  GaMetricDimension,
} from './untrusted_ga_helper.js';
import {VideoProcessorHelper} from './untrusted_video_processor_helper.js';
import {expandPath} from './util.js';
import {WaitableEvent} from './waitable_event.js';

interface UntrustedIFrame {
  iframe: HTMLIFrameElement;
  pageReadyEvent: WaitableEvent;
}

/**
 * Creates the iframe to host the untrusted scripts under
 * chrome-untrusted://camera-app and append to the document.
 */
export function createUntrustedIframe(): UntrustedIFrame {
  const untrustedPageReady = new WaitableEvent();
  const iframe = document.createElement('iframe');
  iframe.addEventListener('load', () => untrustedPageReady.signal());
  if (isLocalDev()) {
    iframe.setAttribute(
        'src', expandPath('/views/untrusted_script_loader.html'));
  } else {
    iframe.setAttribute(
        'src',
        'chrome-untrusted://camera-app/views/untrusted_script_loader.html');
  }
  iframe.hidden = true;
  document.body.appendChild(iframe);
  return {iframe, pageReadyEvent: untrustedPageReady};
}

// TODO(pihsun): actually get correct type from the function definition.
interface UntrustedScriptLoader {
  loadScript(url: string): Promise<void>;
}

/**
 * Creates JS module by given |scriptUrl| under untrusted context with given
 * origin and returns its proxy.
 *
 * @param untrustedIframe The untrusted iframe.
 * @param scriptUrl The URL of the script to load.
 */
export async function injectUntrustedJSModule<T>(
    untrustedIframe: UntrustedIFrame,
    scriptUrl: string): Promise<Comlink.Remote<T>> {
  const {iframe, pageReadyEvent} = untrustedIframe;
  await pageReadyEvent.wait();
  assert(iframe.contentWindow !== null);
  const untrustedRemote = Comlink.wrap<UntrustedScriptLoader>(
      Comlink.windowEndpoint(iframe.contentWindow, self));
  await untrustedRemote.loadScript(scriptUrl);
  // loadScript adds the script exports to what's exported by the
  // untrustedRemote, so we manually cast it to the expected type.
  // eslint-disable-next-line @typescript-eslint/consistent-type-assertions
  return untrustedRemote as unknown as Comlink.Remote<T>;
}

let gaHelper: Promise<Comlink.Remote<GaHelper>>|null = null;
let videoProcessorHelper: Promise<Comlink.Remote<VideoProcessorHelper>>|null =
    null;

/**
 * Gets the singleton GaHelper instance that is located in an untrusted iframe.
 */
export function getGaHelper(): Promise<Comlink.Remote<GaHelper>> {
  return assertExists(gaHelper);
}

/**
 * Sets the singleton GaHelper instance. This should only be called on
 * initialize by init.ts.
 */
export function setGaHelper(newGaHelper: Promise<Comlink.Remote<GaHelper>>):
    void {
  assert(gaHelper === null, 'gaHelper should only be initialize once on init');
  gaHelper = newGaHelper;
}

/**
 * Types of event parameters and dimensions for GA and GA4.
 */
export {Ga4MetricDimension, GaMetricDimension};
export type{Ga4EventParams, GaBaseEvent};

/**
 * Gets the singleton VideoProcessorHelper instance that is located in an
 * untrusted iframe.
 */
export function getVideoProcessorHelper():
    Promise<Comlink.Remote<VideoProcessorHelper>> {
  return assertExists(videoProcessorHelper);
}

/**
 * Sets the singleton VideoProcessorHelper instance. This should only be called
 * on initialize by init.ts.
 */
export function setVideoProcessorHelper(
    newVideoProcessorHelper: Promise<Comlink.Remote<VideoProcessorHelper>>):
    void {
  assert(
      videoProcessorHelper === null,
      'videoProcessorHelper should only be initialize once on init');
  videoProcessorHelper = newVideoProcessorHelper;
}
