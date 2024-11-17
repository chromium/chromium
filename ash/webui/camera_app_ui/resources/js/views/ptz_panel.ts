// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert, assertExists, assertInstanceof} from '../assert.js';
import {AsyncJobQueue} from '../async_job_queue.js';
import {PtzController} from '../device/ptz_controller.js';
import * as dom from '../dom.js';
import * as metrics from '../metrics.js';
import * as state from '../state.js';
import {ViewName} from '../type.js';
import {DelayInterval} from '../util.js';

import {EnterOptions, PtzPanelOptions, View} from './view.js';

/**
 * Detects hold gesture on UI and triggers corresponding handler.
 *
 * @param params Gesture parameters.
 * @param params.button Target button for the gesture.
 * @param params.handlePress Triggered once for the first press.
 * @param params.handleHold Triggered every |holdInterval| ms when holding UI
 *     for more than |pressTimeout| ms.
 * @param params.handleRelease Triggered once the user releases the button.
 * @param params.pressTimeout Timeout in ms before triggering |handleHold|.
 * @param params.holdInterval Trigger interval for the |handleHold|.
 */
function detectHoldGesture({
  button,
  handlePress,
  handleHold,
  handleRelease,
  pressTimeout,
  holdInterval,
}: {
  button: HTMLButtonElement,
  handlePress: () => void,
  handleHold: () => void,
  handleRelease: () => void,
  pressTimeout: number,
  holdInterval: number,
}) {
  let interval: DelayInterval|null = null;

  function press() {
    if (interval !== null) {
      interval.stop();
    }
    handlePress();
    interval = new DelayInterval(() => {
      if (button.disabled) {
        // Releasing the hold if the button is disabled, since disabled button
        // might not get onkeyup event.
        release();
        return;
      }
      handleHold();
    }, pressTimeout, holdInterval);
  }

  function release() {
    if (interval !== null) {
      interval.stop();
      interval = null;
    }
    handleRelease();
  }

  button.onpointerdown = press;
  button.onpointerleave = release;
  button.onpointerup = release;
  button.onkeydown = ({key, repeat}) => {
    if (repeat) {
      // Ignoring repeating keydown event since we have our own DelayInterval
      // implementation.
      return;
    }
    if (key === 'Enter' || key === ' ') {
      press();
    }
  };
  button.onkeyup = ({key}) => {
    if (key === 'Enter' || key === ' ') {
      release();
    }
  };
  // Prevent context menu popping out when touch hold buttons.
  button.oncontextmenu = () => false;
}

/**
 * View controller for PTZ panel.
 */
export class PtzPanel extends View {
  private ptzController: PtzController|null = null;

  private readonly panel = dom.get('#ptz-panel', HTMLDivElement);

  private readonly resetAll = dom.get('#ptz-reset-all', HTMLButtonElement);

  private readonly panLeft = dom.get('#pan-left', HTMLButtonElement);

  private readonly panRight = dom.get('#pan-right', HTMLButtonElement);

  private readonly tiltUp = dom.get('#tilt-up', HTMLButtonElement);

  private readonly tiltDown = dom.get('#tilt-down', HTMLButtonElement);

  private readonly zoomIn = dom.get('#zoom-in', HTMLButtonElement);

  private readonly zoomOut = dom.get('#zoom-out', HTMLButtonElement);

  private mirrorObserver: ((mirror: boolean) => void)|null = null;

  /**
   * Queues asynchronous pan change jobs in sequence.
   */
  private panQueues = new AsyncJobQueue();

  /**
   * Queues asynchronous tilt change jobs in sequence.
   */
  private tiltQueues = new AsyncJobQueue();

  /**
   * Queues asynchronous zoom change jobs in sequence.
   */
  private zoomQueues = new AsyncJobQueue();

  /**
   * Whether the camera associated with current track is a camera whose PT
   * control is disabled when all zooming out.
   */
  private isPanTiltRestricted = false;

  constructor() {
    super(ViewName.PTZ_PANEL, {
      dismissByEsc: true,
      dismissByBackgroundClick: true,
      dismissOnStopStreaming: true,
    });

    this.setMirrorObserver(() => {
      this.checkDisabled();
    });
  }

  private removeMirrorObserver() {
    if (this.mirrorObserver !== null) {
      state.removeObserver(state.State.MIRROR, this.mirrorObserver);
    }
  }

  private setMirrorObserver(observer: (mirror: boolean) => void) {
    this.removeMirrorObserver();
    this.mirrorObserver = observer;
    state.addObserver(state.State.MIRROR, observer);
  }

  /**
   * Binds buttons with the attribute name to be controlled.
   *
   * @param attr One of pan, tilt, zoom attribute name to be bound.
   * @param incBtn Button for increasing the value.
   * @param decBtn Button for decreasing the value.
   */
  private bind(
      attr: 'pan'|'tilt'|'zoom', incBtn: HTMLButtonElement,
      decBtn: HTMLButtonElement): AsyncJobQueue {
    const ptzController = assertExists(this.ptzController);
    const {min, max, step} = ptzController.getCapabilities()[attr];
    function getCurrent() {
      return assertExists(ptzController.getSettings()[attr]);
    }
    this.checkDisabled();

    const queue = new AsyncJobQueue();

    /**
     * Returns a function triggering |attr| change of preview moving toward
     * +1/-1 direction with |deltaInPercent|.
     *
     * @param deltaInPercent Change rate in percent with respect to min/max
     *     range.
     * @param direction Change in +1 or -1 direction.
     */
    const onTrigger = (deltaInPercent: number, direction: number): () =>
        void => {
          const delta =
              Math.max(
                  Math.round((max - min) / step * deltaInPercent / 100), 1) *
              step * direction;
          return () => {
            queue.push(async () => {
              const current = getCurrent();
              const needMirror =
                  attr === 'pan' && state.get(state.State.MIRROR);
              const next = Math.max(
                  min, Math.min(max, current + delta * (needMirror ? -1 : 1)));
              if (current === next) {
                return;
              }
              // Apply pan, tilt, or zoom with |next| value.
              await ptzController[attr](next);
              this.checkDisabled();
            });
          };
        };

    const PRESS_TIMEOUT = 500;
    const HOLD_INTERVAL = 200;
    const pressStepPercent = attr === 'zoom' ? 10 : 1;
    const holdStepPercent = HOLD_INTERVAL / 1000;  // Move 1% in 1000 ms.
    detectHoldGesture({
      button: incBtn,
      handlePress: onTrigger(pressStepPercent, 1),
      handleHold: onTrigger(holdStepPercent, 1),
      handleRelease: () => queue.clear(),
      pressTimeout: PRESS_TIMEOUT,
      holdInterval: HOLD_INTERVAL,
    });
    detectHoldGesture({
      button: decBtn,
      handlePress: onTrigger(pressStepPercent, -1),
      handleHold: onTrigger(holdStepPercent, -1),
      handleRelease: () => queue.clear(),
      pressTimeout: PRESS_TIMEOUT,
      holdInterval: HOLD_INTERVAL,
    });

    return queue;
  }

  private checkDisabled() {
    if (this.ptzController === null) {
      return;
    }
    const capabilities = this.ptzController.getCapabilities();
    const settings = this.ptzController.getSettings();
    function updateDisable(
        incBtn: HTMLButtonElement, decBtn: HTMLButtonElement,
        attr: 'pan'|'tilt'|'zoom') {
      const current = settings[attr];
      const {min, max, step} = capabilities[attr];
      assert(current !== undefined);
      decBtn.disabled = current - step < min;
      incBtn.disabled = current + step > max;
    }
    if (capabilities.zoom !== undefined) {
      updateDisable(this.zoomIn, this.zoomOut, 'zoom');
    }
    const allZoomOut = this.zoomOut.disabled;

    if (capabilities.tilt !== undefined) {
      if (allZoomOut && this.isPanTiltRestricted) {
        this.tiltUp.disabled = this.tiltDown.disabled = true;
      } else {
        updateDisable(this.tiltUp, this.tiltDown, 'tilt');
      }
    }
    if (capabilities.pan !== undefined) {
      if (allZoomOut && this.isPanTiltRestricted) {
        this.panLeft.disabled = this.panRight.disabled = true;
      } else {
        let incBtn = this.panRight;
        let decBtn = this.panLeft;
        if (state.get(state.State.MIRROR)) {
          ([incBtn, decBtn] = [decBtn, incBtn]);
        }
        updateDisable(incBtn, decBtn, 'pan');
      }
    }
  }

  override entering(options: EnterOptions): void {
    const {ptzController} = assertInstanceof(options, PtzPanelOptions);
    const {bottom, right} =
        dom.get('#open-ptz-panel', HTMLButtonElement).getBoundingClientRect();
    this.panel.style.bottom = `${window.innerHeight - bottom}px`;
    this.panel.style.left = `${right + 6}px`;
    this.isPanTiltRestricted = ptzController.isPanTiltRestricted();
    this.ptzController = ptzController;

    const canPan = ptzController.canPan();
    const canTilt = ptzController.canTilt();
    const canZoom = ptzController.canZoom();

    metrics.sendOpenPTZPanelEvent({
      pan: canPan,
      tilt: canTilt,
      zoom: canZoom,
    });

    state.set(state.State.HAS_PAN_SUPPORT, canPan);
    state.set(state.State.HAS_TILT_SUPPORT, canTilt);
    state.set(state.State.HAS_ZOOM_SUPPORT, canZoom);

    if (canPan) {
      this.panQueues = this.bind('pan', this.panRight, this.panLeft);
    }

    if (canTilt) {
      this.tiltQueues = this.bind('tilt', this.tiltUp, this.tiltDown);
    }

    if (canZoom) {
      this.zoomQueues = this.bind('zoom', this.zoomIn, this.zoomOut);
    }

    this.resetAll.onclick = async () => {
      await Promise.all([
        this.panQueues.clear(),
        this.tiltQueues.clear(),
        this.zoomQueues.clear(),
      ]);
      await ptzController.resetPtz();
      this.checkDisabled();
    };
  }

  override leaving(): boolean {
    this.removeMirrorObserver();
    return true;
  }
}
