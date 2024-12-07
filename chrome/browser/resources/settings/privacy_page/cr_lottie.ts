// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'cr-lottie' is a wrapper around the player for lottie
 * animations. Since the player runs on a worker thread, 'cr-lottie' requires
 * the document CSP to be set to "worker-src blob: chrome://resources 'self';".
 *
 * For documents that have TrustedTypes CSP checks enabled, it also requires the
 * document CSP to be set to "trusted-types lottie-worker-script-loader;".
 *
 * Fires a 'cr-lottie-initialized' event when the animation was successfully
 * initialized.
 * Fires a 'cr-lottie-playing' event when the animation starts playing.
 * Fires a 'cr-lottie-paused' event when the animation has paused.
 * Fires a 'cr-lottie-stopped' event when animation has stopped.
 * Fires a 'cr-lottie-resized' event when the canvas the animation is being
 * drawn on is resized.
 */

import {assert, assertNotReached} from '//resources/js/assert.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from '//resources/lit/v3_0/lit.rollup.js';

import {getCss} from './cr_lottie.css.js';
import {getHtml} from './cr_lottie.html.js';

let workerLoaderPolicy: TrustedTypePolicy|null = null;

function getLottieWorkerURL(): TrustedScriptURL {
  if (workerLoaderPolicy === null) {
    workerLoaderPolicy =
        window.trustedTypes!.createPolicy('lottie-worker-script-loader', {
          createScriptURL: (_ignore: string) => {
            const script =
                `import 'chrome://resources/lottie/lottie_worker.min.js';`;
            // CORS blocks loading worker script from a different origin, even
            // if chrome://resources/ is added in the 'worker-src' CSP header.
            // (see https://crbug.com/1385477). Loading scripts as blob and then
            // instantiating it as web worker is possible.
            const blob = new Blob([script], {type: 'text/javascript'});
            return URL.createObjectURL(blob);
          },
          createHTML: () => assertNotReached(),
          createScript: () => assertNotReached(),
        });
  }

  return workerLoaderPolicy.createScriptURL('');
}

interface MessageData {
  animationData: object|null|string;
  drawSize: {width: number, height: number};
  params: {loop: boolean, autoplay: boolean};
  canvas?: OffscreenCanvas;
}

interface CanvasElementWithOffscreen extends HTMLCanvasElement {
  transferControlToOffscreen: () => OffscreenCanvas;
}

export interface CrLottieElement {
  $: {
    canvas: CanvasElementWithOffscreen,
  };
}

export class CrLottieElement extends CrLitElement {
  static get is() {
    return 'cr-lottie';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      animationUrl: {type: String},
      autoplay: {type: Boolean},
      hidden: {type: Boolean},
      singleLoop: {type: Boolean},
    };
  }

  animationUrl: string = '';
  autoplay: boolean = false;
  override hidden: boolean = false;
  singleLoop: boolean = false;

  private canvasElement_: CanvasElementWithOffscreen|null = null;
  private isAnimationLoaded_: boolean = false;
  private offscreenCanvas_: OffscreenCanvas|null = null;

  /** Whether the canvas has been transferred to the worker thread. */
  private hasTransferredCanvas_: boolean = false;

  private resizeObserver_: ResizeObserver|null = null;

  /**
   * The last state that was explicitly set via setPlay.
   * In case setPlay() is invoked before the animation is initialized, the
   * state is stored in this variable. Once the animation initializes, the
   * state is sent to the worker.
   */
  private playState_: boolean = false;

  /**
   * Whether the Worker needs to receive new size
   * information about the canvas. This is necessary for the corner case
   * when the size information is received when the animation is still being
   * loaded into the worker.
   */
  private workerNeedsSizeUpdate_: boolean = false;

  /**
   * Whether the Worker needs to receive new control
   * information about its desired state. This is necessary for the corner
   * case when the control information is received when the animation is still
   * being loaded into the worker.
   */
  private workerNeedsPlayControlUpdate_: boolean = false;

  private worker_: Worker|null = null;

  /** The current in-flight request. */
  private xhr_: XMLHttpRequest|null = null;

  override connectedCallback() {
    super.connectedCallback();

    this.worker_ =
        new Worker(getLottieWorkerURL() as unknown as URL, {type: 'module'});
    this.worker_.onmessage = this.onMessage_.bind(this);
    this.initialize_();
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    if (this.resizeObserver_) {
      this.resizeObserver_.disconnect();
    }
    if (this.worker_) {
      this.worker_.terminate();
      this.worker_ = null;
    }
    if (this.xhr_) {
      this.xhr_.abort();
      this.xhr_ = null;
    }
  }

  /**
   * Updates the animation that is being displayed.
   */
  override updated(changedProperties: PropertyValues<this>) {
    super.updated(changedProperties);
    if (!changedProperties.has('animationUrl')) {
      return;
    }

    if (!this.worker_) {
      // The worker hasn't loaded yet. We will load the new animation once the
      // worker loads.
      return;
    }
    if (this.xhr_) {
      // There is an in-flight request to load the previous animation. Abort it
      // before loading a new image.
      this.xhr_.abort();
      this.xhr_ = null;
    }
    if (this.isAnimationLoaded_) {
      this.worker_.postMessage({control: {stop: true}});
      this.isAnimationLoaded_ = false;
    }
    this.sendXmlHttpRequest_(
        this.animationUrl, 'json', this.initAnimation_.bind(this));
  }

  /**
   * Controls the animation based on the value of |shouldPlay|. If the
   * animation is being loaded into the worker when this method is invoked,
   * the action will be postponed to when the animation is fully loaded.
   * @param shouldPlay True for play, false for pause.
   */
  setPlay(shouldPlay: boolean) {
    this.playState_ = shouldPlay;
    if (this.isAnimationLoaded_) {
      this.sendPlayControlInformationToWorker_();
    } else {
      this.workerNeedsPlayControlUpdate_ = true;
    }
  }

  /**
   * Sends control (play/pause) information to the worker.
   */
  private sendPlayControlInformationToWorker_() {
    assert(this.worker_);
    this.worker_.postMessage({control: {play: this.playState_}});
  }

  /**
   * Initializes all the members of this element.
   */
  private initialize_() {
    // Generate an offscreen canvas.
    this.canvasElement_ = this.$.canvas;
    this.offscreenCanvas_ = this.canvasElement_.transferControlToOffscreen();

    this.resizeObserver_ =
        new ResizeObserver(this.onCanvasElementResized_.bind(this));
    this.resizeObserver_.observe(this.canvasElement_);

    if (this.isAnimationLoaded_) {
      return;
    }

    // Open animation file and start playing the animation.
    this.sendXmlHttpRequest_(
        this.animationUrl, 'json', this.initAnimation_.bind(this));
  }

  /**
   * Computes the draw buffer size for the canvas. This ensures that the
   * rasterization is crisp and sharp rather than blurry.
   * @return Size of the canvas draw buffer
   */
  private getCanvasDrawBufferSize_(): {width: number, height: number} {
    const canvasElement = this.$.canvas;
    const devicePixelRatio = window.devicePixelRatio;
    const clientRect = canvasElement.getBoundingClientRect();
    const drawSize = {
      width: clientRect.width * devicePixelRatio,
      height: clientRect.height * devicePixelRatio,
    };
    return drawSize;
  }

  /**
   * Returns true if the |maybeValidUrl| provided is safe to use in an
   * XMLHTTPRequest.
   * @param maybeValidUrl The url string to check for validity.
   */
  private isValidUrl_(maybeValidUrl: string): boolean {
    const url = new URL(maybeValidUrl, document.location.href);
    return url.protocol === 'chrome:' ||
        (url.protocol === 'data:' &&
         url.pathname.startsWith('application/json;'));
  }

  /**
   * Sends an XMLHTTPRequest to load a resource and runs the callback on
   * getting a successful response.
   * @param url The URL to load the resource.
   * @param responseType The type of response the request would
   *     give on success.
   * @param successCallback The callback to run
   *     when a successful response is received.
   */
  private sendXmlHttpRequest_(
      url: string, responseType: XMLHttpRequestResponseType,
      successCallback: (p: object|null|Blob|MediaSource) => void) {
    assert(this.isValidUrl_(url), 'Invalid scheme or data url used.');
    assert(!this.xhr_);

    this.xhr_ = new XMLHttpRequest();
    this.xhr_!.open('GET', url, true);
    this.xhr_!.responseType = responseType;
    this.xhr_!.send();
    this.xhr_!.onreadystatechange = () => {
      assert(this.xhr_);
      if (this.xhr_.readyState === 4 && this.xhr_.status === 200) {
        // |successCallback| might trigger another xhr, so we set to null before
        // calling it.
        const response = this.xhr_.response;
        this.xhr_ = null;
        successCallback(response);
      }
    };
  }

  /**
   * Handles the canvas element resize event. If the animation isn't fully
   * loaded, the canvas size is sent later, once the loading is done.
   */
  private onCanvasElementResized_() {
    if (this.isAnimationLoaded_) {
      this.sendCanvasSizeToWorker_();
    } else {
      // Mark a size update as necessary once the animation is loaded.
      this.workerNeedsSizeUpdate_ = true;
    }
  }

  /**
   * This informs the offscreen canvas worker of the current canvas size.
   */
  private sendCanvasSizeToWorker_() {
    assert(this.worker_);
    this.worker_.postMessage({drawSize: this.getCanvasDrawBufferSize_()});
  }

  /**
   * Initializes the the animation on the web worker with the data provided.
   * @param animationData The animation that will be played.
   */
  private initAnimation_(animationData: object|null|string) {
    const message: MessageData = {
      animationData,
      drawSize: this.getCanvasDrawBufferSize_(),
      params: {loop: !this.singleLoop, autoplay: this.autoplay},
    };
    assert(this.worker_);
    if (!this.hasTransferredCanvas_) {
      message.canvas = this.offscreenCanvas_!;
      this.hasTransferredCanvas_ = true;
      this.worker_.postMessage(
          message, [this.offscreenCanvas_! as unknown as Transferable]);
    } else {
      this.worker_.postMessage(message);
    }
  }

  /**
   * Handles the messages sent from the web worker to its parent thread.
   * @param event Event sent by the web worker.
   */
  private onMessage_(event: MessageEvent) {
    if (event.data.name === 'initialized' && event.data.success) {
      this.isAnimationLoaded_ = true;
      this.sendPendingInfo_();
      this.fire('cr-lottie-initialized');
    } else if (event.data.name === 'playing') {
      this.fire('cr-lottie-playing');
    } else if (event.data.name === 'paused') {
      this.fire('cr-lottie-paused');
    } else if (event.data.name === 'stopped') {
      this.fire('cr-lottie-stopped');
    } else if (event.data.name === 'resized') {
      this.fire('cr-lottie-resized', event.data.size);
    }
  }

  /**
   * Called once the animation is fully loaded into the worker. Sends any
   * size or control information that may have arrived while the animation
   * was not yet fully loaded.
   */
  private sendPendingInfo_() {
    if (this.workerNeedsSizeUpdate_) {
      this.workerNeedsSizeUpdate_ = false;
      this.sendCanvasSizeToWorker_();
    }
    if (this.workerNeedsPlayControlUpdate_) {
      this.workerNeedsPlayControlUpdate_ = false;
      this.sendPlayControlInformationToWorker_();
    }
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'cr-lottie': CrLottieElement;
  }
}

customElements.define(CrLottieElement.is, CrLottieElement);
