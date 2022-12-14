// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {
  getDefaultWindowSize,
} from './app_window.js';
import {assert, assertInstanceof} from './assert.js';
import * as customEffect from './custom_effect.js';
import {DEPLOYED_VERSION} from './deployed_version.js';
import {CameraManager} from './device/index.js';
import {ModeConstraints} from './device/type.js';
import * as dom from './dom.js';
import {reportError} from './error.js';
import * as expert from './expert.js';
import {GalleryButton} from './gallerybutton.js';
import {I18nString} from './i18n_string.js';
import {Intent} from './intent.js';
import * as metrics from './metrics.js';
import * as filesystem from './models/file_system.js';
import * as loadTimeData from './models/load_time_data.js';
import * as localStorage from './models/local_storage.js';
import {ChromeHelper} from './mojo/chrome_helper.js';
import {DeviceOperator} from './mojo/device_operator.js';
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
import {Dialog} from './views/dialog.js';
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
    windowController.addListener(() => nav.layoutShownViews());

    customEffect.setup();
    util.setupI18nElements(document.body);
    this.setupToggles();
    localStorage.cleanup();
    this.setupEffect();

    // Set up views navigation by their DOM z-order.
    nav.setup([
      this.cameraView,
      new Warning(),
      new Dialog(ViewName.MESSAGE_DIALOG),
      new View(ViewName.SPLASH),
    ]);

    nav.open(ViewName.SPLASH);
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
      const localStorageKey = element.dataset['key'] === undefined ?
          null :
          util.assertEnumVariant(LocalStorageKey, element.dataset['key']);
      const stateKey = element.dataset['state'] === undefined ?
          null :
          state.assertState(element.dataset['state']);

      function save(element: HTMLInputElement) {
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
      if (localStorageKey !== null) {
        const value = localStorage.getBool(localStorageKey, element.checked);
        util.toggleChecked(element, value);
      }
    }
  }

  /**
   * Sets up visual effects, toasts, and dialogs for the new features.
   */
  async setupFeatureEffectsAndDialogs(): Promise<void> {
    const registerDocScanIntroductionDialog = () => {
      this.cameraManager.registerCameraUI({
        onUpdateConfig: async () => {
          if (localStorage.getBool(LocalStorageKey.DOC_MODE_DIALOG_SHOWN) ||
              !state.get(Mode.SCAN)) {
            return;
          }

          const {ready} =
              await ChromeHelper.getInstance().getDocumentScannerReadyState();
          if (!ready) {
            return;
          }
          // No need to show doc scan feature toast if the user has already seen
          // the doc scan mode.
          localStorage.set(LocalStorageKey.DOC_MODE_TOAST_SHOWN, true);

          localStorage.set(LocalStorageKey.DOC_MODE_DIALOG_SHOWN, true);
          const message = loadTimeData.getI18nMessage(
              I18nString.DOCUMENT_MODE_DIALOG_INTRO_TITLE);
          nav.open(ViewName.DOCUMENT_MODE_DIALOG, {message});
        },
      });
    };
    const registerDownloadingDocScanIndicator = () => {
      let hasShownDocIndicator = false;
      this.cameraManager.registerCameraUI({
        onUpdateConfig: async () => {
          const {ready} =
              await ChromeHelper.getInstance().getDocumentScannerReadyState();
          if (ready || !state.get(Mode.SCAN) || hasShownDocIndicator) {
            return;
          }
          customEffect.showDownloadingDocScanIndicator(this.cameraView.root);
          hasShownDocIndicator = true;
        },
      });
    };
    const registerPtzToast = () => {
      this.cameraManager.registerCameraUI({
        onUpdateConfig: () => {
          if (state.get(state.State.ENABLE_PTZ) &&
              !localStorage.getBool(LocalStorageKey.PTZ_TOAST_SHOWN)) {
            localStorage.set(LocalStorageKey.PTZ_TOAST_SHOWN, true);
            customEffect.showPtzToast(this.cameraView.root);
          }
        },
      });
    };

    const {supported, ready} =
        await ChromeHelper.getInstance().getDocumentScannerReadyState();

    // Handling logic for new feature toast.
    if (supported && ready &&
        !localStorage.getBool(LocalStorageKey.DOC_MODE_TOAST_SHOWN)) {
      // Only show new feature indicator for doc scan if it is ready when
      // starting the app.
      localStorage.set(LocalStorageKey.DOC_MODE_TOAST_SHOWN, true);
      customEffect.showDocScanAvailableIndicator(this.cameraView.root);
    } else if (!localStorage.getBool(LocalStorageKey.PTZ_TOAST_SHOWN)) {
      if (state.get(state.State.ENABLE_PTZ)) {
        localStorage.set(LocalStorageKey.PTZ_TOAST_SHOWN, true);
        customEffect.showPtzToast(this.cameraView.root);
      } else {
        registerPtzToast();
      }
    }


    // TODO(chuhsuan): Separate loading indicators and feature toasts in
    // order to provide more control like showing them at the same time.
    if (supported) {
      if (!localStorage.getBool(LocalStorageKey.DOC_MODE_DIALOG_SHOWN)) {
        registerDocScanIntroductionDialog();
      }
      if (!ready) {
        registerDownloadingDocScanIndicator();
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
        this.intent.finish();
      }
    })();

    const cameraResourceInitialized = new WaitableEvent();
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
    await ChromeHelper.getInstance().initCameraUsageMonitor(
        exploitUsage, releaseUsage);

    const startCamera = (async () => {
      await cameraResourceInitialized.wait();
      const isSuccess = await this.cameraManager.requestResume();

      if (isSuccess) {
        const {aspectRatio} = this.cameraManager.getPreviewResolution();
        const {width, height} = getDefaultWindowSize(aspectRatio);
        window.resizeTo(width, height);
      }

      nav.close(ViewName.SPLASH);
      nav.open(ViewName.CAMERA);

      const windowCreationTime = window.windowCreationTime;
      this.perfLogger.start(
          PerfEvent.LAUNCHING_FROM_WINDOW_CREATION, windowCreationTime);
      this.perfLogger.stop(
          PerfEvent.LAUNCHING_FROM_WINDOW_CREATION, {hasError: !isSuccess});
      if (appWindow !== null) {
        appWindow.onAppLaunched();
      }
    })();

    const preloadImages = (async () => {
      function loadImage(url: string) {
        return new Promise<void>((resolve, reject) => {
          const link = document.createElement('link');
          link.rel = 'preload';
          link.as = 'image';
          link.href = url;
          link.onload = () => resolve();
          link.onerror = () =>
              reject(new Error(`Failed to preload image ${url}`));
          document.head.appendChild(link);
        });
      }
      const results = await Promise.allSettled(
          preloadImagesList.map((name) => loadImage(`/images/${name}`)));
      for (const result of results) {
        if (result.status === 'rejected') {
          reportError(
              ErrorType.PRELOAD_IMAGE_FAILURE, ErrorLevel.ERROR,
              assertInstanceof(result.reason, Error));
          break;
        }
      }
    })();

    metrics.sendLaunchEvent({launchType});
    await Promise.all([showWindow, startCamera, preloadImages]);
    await this.setupFeatureEffectsAndDialogs();
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
