// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertInstanceof} from '../assert.js';
import {AsyncJobQueue} from '../async_job_queue.js';
import * as dom from '../dom.js';
import * as focusRing from '../focus_ring.js';
import * as metrics from '../metrics.js';
import * as nav from '../nav.js';
import * as state from '../state.js';
import * as tooltip from '../tooltip.js';
import {ViewName} from '../type.js';
import {DelayInterval} from '../util.js';

import {PTZPanelOptions, View} from './view.js';

/**
 * A set of vid:pid of digital zoom cameras whose PT control is disabled when
 * all zooming out.
 * @const {!Set<string>}
 */
const digitalZoomCameras = new Set([
  '046d:0809',
  '046d:0823',
  '046d:0825',
  '046d:082d',
  '046d:0843',
  '046d:085c',
  '046d:085e',
  '046d:0893',
]);

/**
 * Detects hold gesture on UI and triggers corresponding handler.
 * @param {{
 *   button: !HTMLButtonElement,
 *   handlePress: function(): *,
 *   handleHold: function(): *,
 *   handleRelease: function(): *,
 *   pressTimeout: number,
 *   holdInterval: number,
 * }} params For the first press, triggers |handlePress| handler once. When
 * holding UI for more than |pressTimeout| ms, triggers |handleHold| handler
 * every |holdInterval| ms. Triggers |handleRelease| once the user releases the
 * button.
 */
function detectHoldGesture({
  button,
  handlePress,
  handleHold,
  handleRelease,
  pressTimeout,
  holdInterval,
}) {
  /**
   * @type {?DelayInterval}
   */
  let interval = null;

  const press = () => {
    if (interval !== null) {
      interval.stop();
    }
    handlePress();
    interval = new DelayInterval(handleHold, pressTimeout, holdInterval);
  };

  const release = () => {
    if (interval !== null) {
      interval.stop();
      interval = null;
    }
    handleRelease();
  };

  button.onpointerdown = press;
  button.onpointerleave = release;
  button.onpointerup = release;
  button.onkeydown = ({key}) => {
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
export class PTZPanel extends View {
  /**
   * @public
   */
  constructor() {
    super(
        ViewName.PTZ_PANEL,
        {dismissByEsc: true, dismissByBackgroundClick: true});

    /**
     * Video track of opened stream having PTZ support.
     * @private {?MediaStreamTrack}
     */
    this.track_ = null;

    /**
     * @type {?function(): !Promise}
     * @private
     */
    this.resetPTZ_ = null;

    /**
     * @private {!HTMLDivElement}
     * @const
     */
    this.panel_ = dom.get('#ptz-panel', HTMLDivElement);

    /**
     * @private {!HTMLButtonElement}
     * @const
     */
    this.resetAll_ = dom.get('#ptz-reset-all', HTMLButtonElement);

    /**
     * @private {!HTMLButtonElement}
     * @const
     */
    this.panLeft_ = dom.get('#pan-left', HTMLButtonElement);

    /**
     * @private {!HTMLButtonElement}
     * @const
     */
    this.panRight_ = dom.get('#pan-right', HTMLButtonElement);

    /**
     * @private {!HTMLButtonElement}
     * @const
     */
    this.tiltUp_ = dom.get('#tilt-up', HTMLButtonElement);

    /**
     * @private {!HTMLButtonElement}
     * @const
     */
    this.tiltDown_ = dom.get('#tilt-down', HTMLButtonElement);

    /**
     * @private {!HTMLButtonElement}
     * @const
     */
    this.zoomIn_ = dom.get('#zoom-in', HTMLButtonElement);

    /**
     * @private {!HTMLButtonElement}
     * @const
     */
    this.zoomOut_ = dom.get('#zoom-out', HTMLButtonElement);

    /**
     * @type {?function(boolean): void}
     * @private
     */
    this.mirrorObserver_ = null;

    /**
     * Queues asynchronous pan change jobs in sequence.
     * @type {!AsyncJobQueue}
     * @private
     */
    this.panQueues_ = new AsyncJobQueue();

    /**
     * Queues asynchronous tilt change jobs in sequence.
     * @type {!AsyncJobQueue}
     * @private
     */
    this.tiltQueues_ = new AsyncJobQueue();

    /**
     * Queues asynchronous zoom change jobs in sequence.
     * @type {!AsyncJobQueue}
     * @private
     */
    this.zoomQueues_ = new AsyncJobQueue();

    /**
     * Whether the camera associated with current track is a digital zoom
     * cameras whose PT control is disabled when all zooming out.
     * @type {boolean}
     * @private
     */
    this.isDigitalZoom_ = false;

    [this.panLeft_, this.panRight_, this.tiltUp_, this.tiltDown_, this.zoomIn_,
     this.zoomOut_]
        .forEach((el) => {
          el.addEventListener(
              focusRing.FOCUS_RING_UI_RECT_EVENT_NAME, (evt) => {
                if (!(state.get(state.State.HAS_PAN_SUPPORT) &&
                      state.get(state.State.HAS_TILT_SUPPORT) &&
                      state.get(state.State.HAS_ZOOM_SUPPORT))) {
                  return;
                }
                const style = getComputedStyle(el, '::before');
                const getStyleValue = (attr) => {
                  const px = style.getPropertyValue(attr);
                  return Number(px.replace(/^([\d.]+)px$/, '$1'));
                };
                const pRect = el.getBoundingClientRect();
                focusRing.setUIRect(new DOMRectReadOnly(
                    /* x */ pRect.left + getStyleValue('left'),
                    /* y */ pRect.top + getStyleValue('top'),
                    getStyleValue('width'), getStyleValue('height')));
                evt.preventDefault();
              });
        });

    state.addObserver(state.State.STREAMING, (streaming) => {
      if (!streaming && state.get(this.name)) {
        nav.close(this.name);
      }
    });

    [this.panRight_, this.panLeft_, this.tiltUp_, this.tiltDown_].forEach(
        (btn) => {
          btn.addEventListener(tooltip.TOOLTIP_POSITION_EVENT_NAME, (e) => {
            const target = assertInstanceof(e.target, HTMLElement);
            const pRect = target.offsetParent.getBoundingClientRect();
            const style = getComputedStyle(target, '::before');
            const getStyleValue = (attr) => {
              const px = style.getPropertyValue(attr);
              return Number(px.replace(/^([\d.]+)px$/, '$1'));
            };
            const offsetX = getStyleValue('left');
            const offsetY = getStyleValue('top');
            const width = getStyleValue('width');
            const height = getStyleValue('height');
            tooltip.position(new DOMRectReadOnly(
                /* x */ pRect.left + offsetX, /* y */ pRect.top + offsetY,
                width, height));
            e.preventDefault();
          });
        });

    this.setMirrorObserver_(() => {
      this.checkDisabled_();
    });
  }

  /**
   * @private
   */
  removeMirrorObserver_() {
    if (this.mirrorObserver_ !== null) {
      state.removeObserver(state.State.MIRROR, this.mirrorObserver_);
    }
  }

  /**
   * @param {function(boolean): void} observer
   * @private
   */
  setMirrorObserver_(observer) {
    this.removeMirrorObserver_();
    this.mirrorObserver_ = observer;
    state.addObserver(state.State.MIRROR, observer);
  }

  /**
   * Binds buttons with the attribute name to be controlled.
   * @param {string} attr One of pan, tilt, zoom attribute name to be bound.
   * @param {!HTMLButtonElement} incBtn Button for increasing the value.
   * @param {!HTMLButtonElement} decBtn Button for decreasing the value.
   * @return {!AsyncJobQueue}
   */
  bind_(attr, incBtn, decBtn) {
    const {min, max, step} = this.track_.getCapabilities()[attr];
    const getCurrent = () => this.track_.getSettings()[attr];
    this.checkDisabled_();

    const queue = new AsyncJobQueue();

    /**
     * Returns a function triggering |attr| change of preview moving toward
     * +1/-1 direction with |deltaInPercent|.
     * @param {number} deltaInPercent Change rate in percent with respect to
     *     min/max range.
     * @param {number} direction Change in +1 or -1 direction.
     * @return {function(): void}
     */
    const onTrigger = (deltaInPercent, direction) => {
      const delta =
          Math.max(Math.round((max - min) / step * deltaInPercent / 100), 1) *
          step * direction;
      return () => {
        queue.push(async () => {
          if (!this.track_.enabled) {
            return;
          }
          const current = getCurrent();
          const needMirror = attr === 'pan' && state.get(state.State.MIRROR);
          const next = Math.max(
              min, Math.min(max, current + delta * (needMirror ? -1 : 1)));
          if (current === next) {
            return;
          }
          await this.track_.applyConstraints({advanced: [{[attr]: next}]});
          this.checkDisabled_();
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

  /**
   * @return {boolean}
   * @private
   */
  canPan_() {
    return this.track_.getCapabilities().pan !== undefined;
  }

  /**
   * @return {boolean}
   * @private
   */
  canTilt_() {
    return this.track_.getCapabilities().tilt !== undefined;
  }

  /**
   * @return {boolean}
   * @private
   */
  canZoom_() {
    return this.track_.getCapabilities().zoom !== undefined;
  }

  /**
   * @private
   */
  checkDisabled_() {
    if (this.track_ === null) {
      return;
    }
    const capabilities = this.track_.getCapabilities();
    const settings = this.track_.getSettings();
    const updateDisable = (incBtn, decBtn, attr) => {
      const current = settings[attr];
      const {min, max, step} = capabilities[attr];
      decBtn.disabled = current - step < min;
      incBtn.disabled = current + step > max;
    };
    if (capabilities.zoom !== undefined) {
      updateDisable(this.zoomIn_, this.zoomOut_, 'zoom');
    }
    const allZoomOut = this.zoomOut_.disabled;

    if (capabilities.tilt !== undefined) {
      if (allZoomOut && this.isDigitalZoom_) {
        this.tiltUp_.disabled = this.tiltDown_.disabled = true;
      } else {
        updateDisable(this.tiltUp_, this.tiltDown_, 'tilt');
      }
    }
    if (capabilities.pan !== undefined) {
      if (allZoomOut && this.isDigitalZoom_) {
        this.panLeft_.disabled = this.panRight_.disabled = true;
      } else {
        let incBtn = this.panRight_;
        let decBtn = this.panLeft_;
        if (state.get(state.State.MIRROR)) {
          ([incBtn, decBtn] = [decBtn, incBtn]);
        }
        updateDisable(incBtn, decBtn, 'pan');
      }
    }
  }

  /**
   * @override
   */
  entering(options) {
    const {stream, vidPid, resetPTZ} =
        assertInstanceof(options, PTZPanelOptions);
    const {bottom, right} =
        dom.get('#open-ptz-panel', HTMLButtonElement).getBoundingClientRect();
    this.panel_.style.bottom = `${window.innerHeight - bottom}px`;
    this.panel_.style.left = `${right + 6}px`;
    this.track_ = assertInstanceof(stream, MediaStream).getVideoTracks()[0];
    this.isDigitalZoom_ = state.get(state.State.USE_FAKE_CAMERA) ||
        (vidPid !== null && digitalZoomCameras.has(vidPid));
    this.resetPTZ_ = resetPTZ;


    const canPan = this.canPan_();
    const canTilt = this.canTilt_();
    const canZoom = this.canZoom_();

    metrics.sendOpenPTZPanelEvent({
      pan: canPan,
      tilt: canTilt,
      zoom: canZoom,
    });

    state.set(state.State.HAS_PAN_SUPPORT, canPan);
    state.set(state.State.HAS_TILT_SUPPORT, canTilt);
    state.set(state.State.HAS_ZOOM_SUPPORT, canZoom);

    if (canPan) {
      this.panQueues_ = this.bind_('pan', this.panRight_, this.panLeft_);
    }

    if (canTilt) {
      this.tiltQueues_ = this.bind_('tilt', this.tiltUp_, this.tiltDown_);
    }

    if (canZoom) {
      this.zoomQueues_ = this.bind_('zoom', this.zoomIn_, this.zoomOut_);
    }

    this.resetAll_.onclick = async () => {
      await Promise.all([
        this.panQueues_.clear(),
        this.tiltQueues_.clear(),
        this.zoomQueues_.clear(),
      ]);
      await this.resetPTZ_();
      this.checkDisabled_();
    };
  }

  /**
   * @override
   */
  leaving() {
    this.removeMirrorObserver_();
    return true;
  }
}
