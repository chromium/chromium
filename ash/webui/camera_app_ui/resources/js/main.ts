// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {
  startColorChangeUpdater,
} from
    'chrome://resources/cr_components/color_change_listener/colors_css_updater.js';

import {
  getDefaultWindowSize,
} from './app_window.js';
import {assert, assertInstanceof} from './assert.js';
import {DEPLOYED_VERSION} from './deployed_version.js';
import {CameraManager} from './device/index.js';
import {ModeConstraints} from './device/type.js';
import * as dom from './dom.js';
import {reportError} from './error.js';
import * as expert from './expert.js';
import {Flag} from './flag.js';
import {GalleryButton} from './gallerybutton.js';
import {I18nString} from './i18n_string.js';
import {Intent} from './intent.js';
import * as Comlink from './lib/comlink.js';
import {loadSvgImages} from './lit/svg_wrapper.js';
import * as metrics from './metrics.js';
import * as filesystem from './models/file_system.js';
import * as loadTimeData from './models/load_time_data.js';
import * as localStorage from './models/local_storage.js';
import {ChromeHelper} from './mojo/chrome_helper.js';
import {DeviceOperator} from './mojo/device_operator.js';
import {WindowStateType} from './mojo/type.js';
import {WindowInstance} from './multi_window_manager.js';
import * as nav from './nav.js';
import {PerfLogger} from './perf.js';
import {preloadImagesList} from './preload_images.js';
import * as state from './state.js';
import * as toast from './toast.js';
import * as tooltip from './tooltip.js';
import {
  ErrorLevel,
  ErrorType,
  Facing,
  LocalStorageKey,
  Mode,
  PerfEvent,
  ViewName,
} from './type.js';
import {addUnloadCallback} from './unload.js';
import * as util from './util.js';
import {Camera} from './views/camera.js';
import * as timertick from './views/camera/timertick.js';
import {CameraIntent} from './views/camera_intent.js';
import {View} from './views/view.js';
import {Warning, WarningType} from './views/warning.js';
import {WaitableEvent} from './waitable_event.js';
import {windowController} from './window_controller.js';

/**
 * The app window instance which is used for communication with Tast tests. For
 * non-test sessions, it should be null.
 */
const appWindow = window.appWindow;

/**
 * Creates the Camera App main object.
 */
export class App {
  private readonly perfLogger: PerfLogger;

  private readonly intent: Intent|null;

  private readonly cameraManager: CameraManager;

  private readonly galleryButton = new GalleryButton();

  private readonly cameraView: Camera;

  constructor({perfLogger, intent, facing, mode}: {
    perfLogger: PerfLogger,
    intent: Intent|null,
    facing: Facing|null,
    mode: Mode|null,
  }) {
    this.perfLogger = perfLogger;

    this.intent = intent;
    const shouldHandleIntentResult = this.intent?.shouldHandleResult === true;
    state.set(
        state.State.SHOULD_HANDLE_INTENT_RESULT, shouldHandleIntentResult);

    const modeConstraints: ModeConstraints = {
      kind: shouldHandleIntentResult && mode !== null ? 'exact' : 'default',
      mode: mode ?? Mode.PHOTO,
    };
    this.cameraManager =
        new CameraManager(this.perfLogger, facing, modeConstraints);

    this.cameraView = (() => {
      if (shouldHandleIntentResult) {
        // If shouldHandleIntentResult is true, then this.intent is definitely
        // not null.
        assert(this.intent !== null);
        return new CameraIntent(
            this.intent, this.cameraManager, this.perfLogger);
      } else {
        return new Camera(
            this.galleryButton, this.cameraManager, this.perfLogger);
      }
    })();

    document.body.addEventListener(
        'keydown', (event) => this.onKeyPressed(event));

    // Disable the zoom in-out gesture which is triggered by wheel and pinch on
    // trackpad.
    document.body.addEventListener('wheel', (event) => {
      if (event.ctrlKey) {
        event.preventDefault();
      }
    }, {passive: false, capture: true});

    window.addEventListener('resize', () => nav.layoutShownViews());
    windowController.addWindowStateListener(() => nav.layoutShownViews());

    util.setupI18nElements(document.body);
    this.setupTooltip();
    this.setupToggles();
    localStorage.cleanup();
    this.setupEffect();
    this.setupExperimentalFeatures();

    // Set up views navigation by their DOM z-order.
    nav.setup([
      this.cameraView,
      new Warning(),
      new View(ViewName.SPLASH),
    ]);

    nav.open(ViewName.SPLASH);
  }

  /**
   * Sets up tooltips for elements having `i18n-label` attribute. This method
   * also setup tooltips for the elements:
   * * Added to the DOM and have a `i18n-label` attribute.
   * * Newly set with a `i18n-label` attribute.
   *
   * Note `i18n-label` attribute should not be removed from elements.
   */
  private setupTooltip() {
    const tooltipAttribute = 'i18n-label';
    const elements =
        Array.from(dom.getAll(`[${tooltipAttribute}]`, HTMLElement));
    tooltip.setup(elements);
    const observer = new MutationObserver((mutations) => {
      const elements: HTMLElement[] = [];
      for (const mutation of mutations) {
        if (mutation.type === 'childList') {
          for (const node of mutation.addedNodes) {
            if (node instanceof HTMLElement &&
                node.hasAttribute(tooltipAttribute)) {
              elements.push(node);
            }
          }
        } else if (mutation.type === 'attributes') {
          const {target: node, attributeName, oldValue} = mutation;
          if (node instanceof HTMLElement &&
              attributeName === tooltipAttribute && oldValue === null) {
            elements.push(node);
          }
        }
      }
      tooltip.setup(elements);
    });
    observer.observe(document.body, {
      subtree: true,
      childList: true,
      attributes: true,
      attributeOldValue: true,
    });
  }

  /**
   * Sets up toggles (checkbox and radio) by data attributes.
   */
  private setupToggles() {
    for (const element of dom.getAll('input', HTMLInputElement)) {
      element.addEventListener('keypress', (event) => {
        const e = assertInstanceof(event, KeyboardEvent);
        if (util.getKeyboardShortcut(e) === 'Enter') {
          element.click();
        }
      });
      function getKey(element: HTMLInputElement) {
        return element.dataset['key'] === undefined ?
            null :
            util.assertEnumVariant(LocalStorageKey, element.dataset['key']);
      }
      const stateKey = element.dataset['state'] === undefined ?
          null :
          state.assertState(element.dataset['state']);

      function save(element: HTMLInputElement) {
        const localStorageKey = getKey(element);
        if (localStorageKey !== null) {
          localStorage.set(localStorageKey, element.checked);
        }
      }
      element.addEventListener('change', (event) => {
        if (stateKey !== null) {
          state.set(stateKey, element.checked);
        }
        // Check if event is triggered by user on UI.
        if (event.isTrusted) {
          save(element);
          if (element.type === 'radio' && element.checked) {
            // Handle unchecked grouped sibling radios.
            const grouped =
                `input[type=radio][name=${element.name}]:not(:checked)`;
            for (const radio of dom.getAll(grouped, HTMLInputElement)) {
              radio.dispatchEvent(new Event('change'));
              save(radio);
            }
          }
        }
      });
      if (stateKey !== null) {
        state.set(stateKey, element.checked);
        state.addObserver(stateKey, (value) => {
          if (value !== element.checked) {
            util.toggleChecked(element, value);
          }
        });
      }
      const localStorageKey = getKey(element);
      if (localStorageKey !== null) {
        const value = localStorage.getBool(localStorageKey, element.checked);
        util.toggleChecked(element, value);
      }
    }
  }

  /**
   * Sets up visual effect for all applicable elements.
   */
  private setupEffect() {
    for (const el of dom.getAll('.inkdrop', HTMLElement)) {
      util.setInkdropEffect(el);
    }

    const observer = new MutationObserver((mutationList) => {
      for (const mutation of mutationList) {
        assert(mutation.type === 'childList');
        // Only the newly added nodes with inkdrop class are considered here. So
        // simply adding class attribute on existing element will not work.
        for (const node of mutation.addedNodes) {
          if (!(node instanceof HTMLElement)) {
            continue;
          }
          if (node.classList.contains('inkdrop')) {
            util.setInkdropEffect(node);
          }
        }
      }
    });
    observer.observe(document.body, {
      subtree: true,
      childList: true,
    });
  }

  private setupExperimentalFeatures() {
    if (loadTimeData.getChromeFlag(Flag.TIME_LAPSE)) {
      const modeButton = dom.get('#time-lapse-mode', HTMLDivElement);
      modeButton.classList.remove('hidden');
    }
  }

  /**
   * Starts the app by loading the model and opening the camera-view.
   */
  async start(launchType: metrics.LaunchType): Promise<void> {
    await DeviceOperator.initializeInstance();
    document.documentElement.dir = loadTimeData.getTextDirection();
    try {
      await filesystem.initialize();
      const cameraDir = filesystem.getCameraDirectory();
      assert(cameraDir !== null);

      // There are three possible cases:
      // 1. Regular instance
      //      (intent === null)
      // 2. STILL_CAPTURE_CAMERA and VIDEO_CAMERA intents
      //      (intent !== null && shouldHandleResult === false)
      // 3. Other intents
      //      (intent !== null && shouldHandleResult === true)
      // Only (1) and (2) will show gallery button on the UI.
      if (this.intent === null || !this.intent.shouldHandleResult) {
        this.galleryButton.initialize(cameraDir);
      }
    } catch (error) {
      reportError(ErrorType.FILE_SYSTEM_FAILURE, ErrorLevel.ERROR, error);
      nav.open(ViewName.WARNING, WarningType.FILESYSTEM_FAILURE);
    }

    const showWindow = (async () => {
      // For intent only requiring open camera with specific mode without
      // returning the capture result, finish it directly.
      if (this.intent !== null && !this.intent.shouldHandleResult) {
        await this.intent.finish();
      }
    })();

    const cameraResourceInitialized = new WaitableEvent();
    await this.setupMultiWindowHandling(cameraResourceInitialized);

    let cameraStartSuccessful = false;

    const startCamera = (async () => {
      await cameraResourceInitialized.wait();
      cameraStartSuccessful = await this.cameraManager.requestResume();

      if (cameraStartSuccessful) {
        const {aspectRatio} = this.cameraManager.getPreviewResolution();
        const {width, height} = getDefaultWindowSize(aspectRatio);
        window.resizeTo(width, height);
      }
    })();

    preloadImages();
    loadSvgImages();
    metrics.sendLaunchEvent({launchType});
    await Promise.all([showWindow, startCamera]);

    nav.close(ViewName.SPLASH);
    nav.open(ViewName.CAMERA);

    const windowCreationTime = window.windowCreationTime;
    this.perfLogger.start(
        PerfEvent.LAUNCHING_FROM_WINDOW_CREATION, windowCreationTime);
    this.perfLogger.stop(
        PerfEvent.LAUNCHING_FROM_WINDOW_CREATION,
        {hasError: !cameraStartSuccessful});

    if (appWindow !== null) {
      appWindow.onAppLaunched();
    }
  }

  /**
   * Handles pressed keys.
   *
   * @param event Key press event.
   */
  private onKeyPressed(event: Event) {
    tooltip.hide();  // Hide shown tooltip on any keypress.
    nav.onKeyPressed(assertInstanceof(event, KeyboardEvent));
  }

  /**
   * Suspends app and hides app window.
   */
  async suspend(): Promise<void> {
    timertick.cancel();
    await this.cameraManager.requestSuspend();
    nav.open(ViewName.WARNING, WarningType.CAMERA_PAUSED);
  }

  /**
   * Resumes app from suspension and shows app window.
   */
  resume(): void {
    this.cameraManager.requestResume();
    nav.close(ViewName.WARNING, WarningType.CAMERA_PAUSED);
  }

  /**
   * Begins to take photo or recording with the current options, e.g. timer.
   *
   * @param shutterType The shutter is triggered by which shutter type.
   * @return Promise resolved when take action completes.
   *     Returns null if CCA can't start take action.
   */
  beginTake(shutterType: metrics.ShutterType): Promise<void>|null {
    return this.cameraView.beginTake(shutterType);
  }

  async setupMultiWindowHandling(cameraResourceInitialized: WaitableEvent):
      Promise<void> {
    const exploitUsage = async () => {
      if (cameraResourceInitialized.isSignaled()) {
        this.resume();
      } else {
        // CCA must get camera usage for completing its initialization when
        // first launched.
        await this.cameraManager.initialize(this.cameraView);
        await this.cameraView.initialize();
        cameraResourceInitialized.signal();
      }
    };
    const releaseUsage = async () => {
      assert(cameraResourceInitialized.isSignaled());
      await this.suspend();
    };

    const multiWindowManagerPath = '/js/multi_window_manager.js';
    const multiWindowManagerWorker =
        new SharedWorker(multiWindowManagerPath, {type: 'module'});
    const windowInstance =
        Comlink.wrap<WindowInstance>(multiWindowManagerWorker.port);
    addUnloadCallback(() => {
      windowInstance.onWindowClosed().catch((e) => {
        reportError(
            ErrorType.MULTI_WINDOW_HANDLING_FAILURE, ErrorLevel.ERROR,
            assertInstanceof(e, Error));
      });
    });
    await windowInstance.init(
        Comlink.proxy(releaseUsage), Comlink.proxy(exploitUsage));
    await ChromeHelper.getInstance().initCameraWindowController();
    windowController.addWindowStateListener((states) => {
      windowInstance
          .onVisibilityChanged(!states.includes(WindowStateType.MINIMIZED))
          .catch((e) => {
            reportError(
                ErrorType.MULTI_WINDOW_HANDLING_FAILURE, ErrorLevel.ERROR,
                assertInstanceof(e, Error));
          });
    });
    windowController.addWindowFocusListener((isFocused) => {
      // If we change the focus to another CCA window, it should get the camera
      // ownership.
      if (isFocused) {
        windowInstance.onVisibilityChanged(true).catch((e) => {
          reportError(
              ErrorType.MULTI_WINDOW_HANDLING_FAILURE, ErrorLevel.ERROR,
              assertInstanceof(e, Error));
        });
      }
    });
  }
}

/**
 * Parse search params in URL.
 */
function parseSearchParams(): {
  intent: Intent|null,
  facing: Facing|null,
  mode: Mode|null,
  openFrom: string|null,
  autoTake: boolean,
} {
  const url = new URL(window.location.href);
  const params = url.searchParams;

  const facing = util.checkEnumVariant(Facing, params.get('facing'));

  const mode = util.checkEnumVariant(Mode, params.get('mode'));

  const intent = (() => {
    if (params.get('intentId') === null) {
      return null;
    }
    assert(mode !== null);
    return Intent.create(url, mode);
  })();

  const autoTake = params.get('autoTake') === '1';
  const openFrom = params.get('openFrom');

  return {intent, facing, mode, autoTake, openFrom};
}

/**
 * Preload images to avoid flickering.
 * TODO(pihsun): Remove this and stop including .svg file in CCA once all
 * images are migrated to use data-svg / loadSvgImages.
 */
function preloadImages() {
  const imagesContainer = document.createElement('div');
  imagesContainer.id = 'preload-images';
  imagesContainer.hidden = true;
  for (const imageName of preloadImagesList) {
    const img = document.createElement('img');
    const url = `/images/${imageName}`;
    img.onerror = () => {
      reportError(
          ErrorType.PRELOAD_IMAGE_FAILURE, ErrorLevel.ERROR,
          new Error(`Failed to preload image ${url}`));
    };
    img.src = url;
    imagesContainer.appendChild(img);
  }
  document.body.appendChild(imagesContainer);
}

/**
 * Append dynamic color CSS files and setup watcher for color changes.
 */
async function setupDynamicColor(): Promise<void> {
  function loadCSS(url: string): Promise<void> {
    return new Promise((resolve) => {
      const link = document.createElement('link');
      link.rel = 'stylesheet';
      link.href = url;
      link.addEventListener('load', () => resolve());
      document.head.appendChild(link);
    });
  }
  if (loadTimeData.getChromeFlag(Flag.JELLY)) {
    startColorChangeUpdater();
    await loadCSS('chrome://theme/colors.css?sets=ref,sys');
  } else {
    await loadCSS('/css/colors_default.css');
  }
}

/**
 * Singleton of the App object.
 */
let instance: App|null = null;

/**
 * Creates the App object and starts camera stream.
 */
(async () => {
  if (instance !== null) {
    return;
  }

  const perfLogger = new PerfLogger();

  await setupDynamicColor();

  const {intent, facing, mode, autoTake, openFrom} = parseSearchParams();

  state.set(state.State.INTENT, intent !== null);

  addUnloadCallback(() => {
    // For SWA, we don't cancel the unhandled intent here since there is no
    // guarantee that asynchronous calls in unload listener can be executed
    // properly. Therefore, we moved the logic for canceling unhandled intent to
    // Chrome (CameraAppHelper).
    if (appWindow !== null) {
      appWindow.notifyClosed();
    }
  });

  metrics.initMetrics();
  if (appWindow !== null) {
    metrics.setMetricsEnabled(false);
  }

  // Setup listener for performance events.
  perfLogger.addListener(({event, duration, perfInfo}) => {
    metrics.sendPerfEvent({event, duration, perfInfo});

    // Setup for console perf logger.
    if (expert.isEnabled(expert.ExpertOption.PRINT_PERFORMANCE_LOGS)) {
      // eslint-disable-next-line no-console
      console.log(
          '%c%s %s ms %s', 'color: #4E4F97; font-weight: bold;',
          event.padEnd(40), duration.toFixed(0).padStart(4),
          JSON.stringify(perfInfo));
    }

    // Setup for Tast tests logger.
    if (appWindow !== null) {
      appWindow.reportPerf({event, duration, perfInfo});
    }
  });

  state.addObserver(state.State.TAKING, (val, extras) => {
    // 'taking' state indicates either taking photo or video. Skips for
    // video-taking case since we only want to collect the metrics of
    // photo-taking.
    if (state.get(Mode.VIDEO)) {
      return;
    }
    const event = PerfEvent.PHOTO_TAKING;

    if (val) {
      perfLogger.start(event);
    } else {
      perfLogger.stop(event, extras);
    }
  });

  const states = Object.values(PerfEvent);
  for (const event of states) {
    state.addObserver(event, (val, extras) => {
      if (val) {
        perfLogger.start(event);
      } else {
        perfLogger.stop(event, extras);
      }
    });
  }

  if (DEPLOYED_VERSION !== undefined) {
    // eslint-disable-next-line no-console
    console.log(
        `Local override enabled for CCA (${DEPLOYED_VERSION}). ` +
        'To disable local override, ' +
        'remove /etc/camera/cca/js/deployed_version.js on device.');
    toast.showDebugMessage(`Local override enabled (${DEPLOYED_VERSION})`);
  }

  instance = new App({perfLogger, intent, facing, mode});
  await instance.start(
      openFrom === 'assistant' ? metrics.LaunchType.ASSISTANT :
                                 metrics.LaunchType.DEFAULT);

  if (autoTake) {
    const takePromise = instance.beginTake(
        openFrom === 'assistant' ? metrics.ShutterType.ASSISTANT :
                                   metrics.ShutterType.UNKNOWN);
    if (takePromise === null) {
      toast.show(
          mode === Mode.VIDEO ? I18nString.ERROR_MSG_RECORD_START_FAILED :
                                I18nString.ERROR_MSG_TAKE_PHOTO_FAILED);
    } else {
      await takePromise;
    }
  }
})();
