// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {
  ColorChangeUpdater,
} from
    'chrome://resources/cr_components/color_change_listener/colors_css_updater.js';

import {
  getDefaultWindowSize,
} from './app_window.js';
import {
  assert,
  assertEnumVariant,
  assertExists,
  assertInstanceof,
  checkEnumVariant,
} from './assert.js';
import * as customEffect from './custom_effect.js';
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
import {getSanitizedScriptUrl} from './trusted_script_url_policy_util.js';
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
 * Sets up tooltips for elements having `i18n-label` attribute. This method
 * also setup tooltips for the elements:
 * * Added to the DOM and have a `i18n-label` attribute.
 * * Newly set with a `i18n-label` attribute.
 *
 * Note `i18n-label` attribute should not be removed from elements.
 */
function setupTooltip() {
  tooltip.init();
  const tooltipAttribute = 'i18n-label';
  const tooltipAttributeSelector = `[${tooltipAttribute}]`;
  const elements =
      Array.from(dom.getAll(tooltipAttributeSelector, HTMLElement));
  tooltip.setupElements(elements);
  const observer = new MutationObserver((mutations) => {
    const elements: HTMLElement[] = [];
    for (const mutation of mutations) {
      if (mutation.type === 'attributes') {
        // Check newly added attributes on existing elements.
        const {target, oldValue} = mutation;
        if (target instanceof HTMLElement && oldValue === null) {
          elements.push(target);
        }
      } else if (mutation.type === 'childList') {
        const {target} = mutation;
        if (target instanceof HTMLElement) {
          elements.push(
              ...dom.getAllFrom(target, tooltipAttributeSelector, HTMLElement),
          );
        }
      }
    }
    tooltip.setupElements(elements);
  });
  observer.observe(document.body, {
    subtree: true,
    childList: true,
    attributeFilter: [tooltipAttribute],
    attributes: true,
    attributeOldValue: true,
  });
}

/**
 * Sets up toggles (checkbox and radio) by data attributes.
 */
function setupToggles() {
  for (const element of dom.getAll('input', HTMLInputElement)) {
    element.addEventListener('keypress', (event) => {
      if (util.getKeyboardShortcut(event) === 'Enter') {
        element.click();
      }
    });
    function getKey(element: HTMLInputElement) {
      return element.dataset['key'] === undefined ?
          null :
          assertEnumVariant(LocalStorageKey, element.dataset['key']);
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
function setupEffect() {
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

function setupExperimentalFeatures() {
  if (loadTimeData.getChromeFlag(Flag.TIME_LAPSE)) {
    const modeButton = dom.get('#time-lapse-mode', HTMLDivElement);
    modeButton.classList.remove('hidden');
  }
}

/**
 * Handles pressed keys.
 */
function onKeyPressed(event: KeyboardEvent) {
  tooltip.hide();  // Hide shown tooltip on any keypress.
  nav.onKeyPressed(event);
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

  const facing = checkEnumVariant(Facing, params.get('facing'));

  const mode = checkEnumVariant(Mode, params.get('mode'));

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
 */
function preloadImages() {
  const imagesContainer = document.createElement('div');
  imagesContainer.id = 'preload-images';
  imagesContainer.hidden = true;
  for (const imageName of preloadImagesList) {
    const img = document.createElement('img');
    const url = util.expandPath(`/images/${imageName}`);
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
    ColorChangeUpdater.forDocument().start();
    await loadCSS('chrome://theme/colors.css?sets=ref,sys');
  } else {
    await loadCSS(util.expandPath('/css/colors_default.css'));
  }
}

function setupNewFeatureToast(
    cameraManager: CameraManager, cameraView: Camera) {
  // TODO(b/236800499): Remove the toast around 3 milestones after the feature
  // is launched.
  if (loadTimeData.getChromeFlag(Flag.TIME_LAPSE)) {
    cameraManager.registerCameraUI({
      onUpdateConfig: () => {
        if (localStorage.getBool(LocalStorageKey.TIME_LAPSE_DIALOG_SHOWN) ||
            state.get(Mode.VIDEO)) {
          return;
        }
        customEffect.showTimeLapseIntroToast(cameraView.root);
        // Do not show the toast to users who has already seen it.
        localStorage.set(LocalStorageKey.TIME_LAPSE_DIALOG_SHOWN, true);
      },
    });
  }
}

async function setupMultiWindowHandling(
    cameraManager: CameraManager, cameraView: Camera,
    cameraResourceInitialized: WaitableEvent): Promise<void> {
  async function handleResume() {
    try {
      if (cameraResourceInitialized.isSignaled()) {
        cameraManager.requestResume();
        nav.close(ViewName.WARNING, WarningType.CAMERA_PAUSED);
      } else {
        // CCA must get camera usage for completing its initialization when
        // first launched.
        await cameraManager.initialize(cameraView);
        await cameraView.initialize();
        cameraResourceInitialized.signal();
      }
    } catch (e) {
      reportError(
          ErrorType.RESUME_CAMERA_FAILURE, ErrorLevel.ERROR,
          assertInstanceof(e, Error));
    }
  }
  async function handleSuspend() {
    try {
      assert(cameraResourceInitialized.isSignaled());
      timertick.cancel();
      await cameraManager.requestSuspend();
      nav.open(ViewName.WARNING, WarningType.CAMERA_PAUSED);
    } catch (e) {
      reportError(
          ErrorType.SUSPEND_CAMERA_FAILURE, ErrorLevel.ERROR,
          assertInstanceof(e, Error));
    }
  }

  const multiWindowManagerWorker = new SharedWorker(
      getSanitizedScriptUrl('/js/multi_window_manager.js'), {type: 'module'});
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
      Comlink.proxy(handleSuspend), Comlink.proxy(handleResume));
  await ChromeHelper.getInstance().initCameraWindowController();
  windowController.addWindowStateListener((states) => {
    const isMinimizing = states.includes(WindowStateType.MINIMIZED);
    // If the window is minimized while recording time-lapse, the camera
    // usage will not be paused to keep recording.
    if (isMinimizing && state.get(state.State.RECORDING) &&
        state.get(state.State.RECORD_TYPE_TIME_LAPSE)) {
      return;
    }
    windowInstance.onVisibilityChanged(!isMinimizing).catch((e) => {
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

function createPerfLogger(): PerfLogger {
  const perfLogger = new PerfLogger();

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
    window.appWindow?.reportPerf({event, duration, perfInfo});
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

  return perfLogger;
}

function setupSvgs() {
  for (const el of dom.getAll('[data-svg]', HTMLElement)) {
    const imageName = assertExists(el.dataset['svg']);
    const svg = document.createElement('svg-wrapper');
    svg.setAttribute('name', imageName);
    // Prepend the svg so it's on the bottom-most layer and won't be covering
    // other possible children (e.g. inkdrop effect).
    el.prepend(svg);
  }
}

/**
 * Setup Camera App and starts camera stream.
 */
async function main() {
  const {intent, facing, mode, autoTake, openFrom} = parseSearchParams();

  state.set(state.State.INTENT, intent !== null);

  addUnloadCallback(() => {
    // For SWA, we don't cancel the unhandled intent here since there is no
    // guarantee that asynchronous calls in unload listener can be executed
    // properly. Therefore, we moved the logic for canceling unhandled intent to
    // Chrome (CameraAppHelper).
    window.appWindow?.notifyClosed();
  });

  // metrics.ts handle it's ready state inside the module, and we don't want to
  // block CCA by metrics initialization.
  void metrics.initMetrics();
  if (window.appWindow !== null) {
    // Disable metrics when in testing.
    void metrics.setEnabled(false);
  }

  const perfLogger = createPerfLogger();

  // toast and splash style depends on dynamic color css being imported.
  await setupDynamicColor();

  if (DEPLOYED_VERSION !== undefined) {
    // eslint-disable-next-line no-console
    console.log(
        `Local override enabled for CCA (${DEPLOYED_VERSION}). ` +
        'To disable local override, ' +
        'remove /etc/camera/cca/js/deployed_version.js on device.');
    toast.showDebugMessage(`Local override enabled (${DEPLOYED_VERSION})`);
  }

  // There are three possible cases:
  // 1. Regular instance
  //      (intent === null)
  // 2. STILL_CAPTURE_CAMERA and VIDEO_CAMERA intents
  //      (intent !== null && shouldHandleResult === false)
  // 3. Other intents
  //      (intent !== null && shouldHandleResult === true)
  // `shouldHandleIntentResult` will be false in (1) and (2), and gallery
  // button will be shown on the UI.
  const shouldHandleIntentResult = intent?.shouldHandleResult === true;
  state.set(state.State.SHOULD_HANDLE_INTENT_RESULT, shouldHandleIntentResult);

  const modeConstraints: ModeConstraints = {
    kind: shouldHandleIntentResult && mode !== null ? 'exact' : 'default',
    mode: mode ?? Mode.PHOTO,
  };
  const cameraManager = new CameraManager(perfLogger, facing, modeConstraints);

  const galleryButton = new GalleryButton();

  const cameraView = shouldHandleIntentResult ?
      new CameraIntent(intent, cameraManager, perfLogger) :
      new Camera(galleryButton, cameraManager, perfLogger);

  // Set up views navigation by their DOM z-order.
  nav.setup([
    cameraView,
    new Warning(),
    new View(ViewName.SPLASH),
  ]);

  nav.open(ViewName.SPLASH);

  document.documentElement.dir = loadTimeData.getTextDirection();
  // Disable the zoom in-out gesture which is triggered by wheel and pinch on
  // trackpad.
  document.body.addEventListener('wheel', (event) => {
    if (event.ctrlKey) {
      event.preventDefault();
    }
  }, {passive: false, capture: true});

  window.addEventListener('resize', () => nav.layoutShownViews());
  windowController.addWindowStateListener(() => nav.layoutShownViews());

  customEffect.setup();
  util.setupI18nElements(document.body);
  setupTooltip();
  setupToggles();
  localStorage.cleanup();
  setupEffect();
  setupNewFeatureToast(cameraManager, cameraView);
  setupExperimentalFeatures();
  preloadImages();
  setupSvgs();

  const launchType = openFrom === 'assistant' ? metrics.LaunchType.ASSISTANT :
                                                metrics.LaunchType.DEFAULT;

  await DeviceOperator.initializeInstance();
  try {
    await filesystem.initialize();
    const cameraDir = filesystem.getCameraDirectory();
    if (!shouldHandleIntentResult) {
      galleryButton.initialize(cameraDir);
    }
  } catch (error) {
    reportError(ErrorType.FILE_SYSTEM_FAILURE, ErrorLevel.ERROR, error);
    nav.open(ViewName.WARNING, WarningType.FILESYSTEM_FAILURE);
  }

  // Create a promise to finish the intent, that runs in parallel with starting
  // camera.
  const finishIntent = (async () => {
    // For intent only requiring open camera with specific mode without
    // returning the capture result, finish it directly.
    if (intent !== null && !intent.shouldHandleResult) {
      await intent.finish();
    }
  })();

  const cameraResourceInitialized = new WaitableEvent();
  await setupMultiWindowHandling(
      cameraManager, cameraView, cameraResourceInitialized);

  // Key handler (in particular, back button) depends on windowController being
  // initialized by setupMultiWindowHandling.
  document.body.addEventListener('keydown', (event) => onKeyPressed(event));

  metrics.sendLaunchEvent({launchType});

  await cameraResourceInitialized.wait();
  const cameraStartSuccessful = await cameraManager.requestResume();

  if (cameraStartSuccessful) {
    const {aspectRatio} = cameraManager.getPreviewResolution();
    const {width, height} = getDefaultWindowSize(aspectRatio);
    window.resizeTo(width, height);
  }

  // Waits for the intent to finish before switching to main camera view.
  // TODO(pihsun): Check if the performance gain for running this in parallel
  // is significant, and simplify this by inlining the promise if it isn't.
  await finishIntent;

  nav.close(ViewName.SPLASH);
  nav.open(ViewName.CAMERA);

  perfLogger.start(
      PerfEvent.LAUNCHING_FROM_WINDOW_CREATION, window.windowCreationTime);
  perfLogger.stop(
      PerfEvent.LAUNCHING_FROM_WINDOW_CREATION,
      {hasError: !cameraStartSuccessful});

  window.appWindow?.onAppLaunched();
  metrics.sendOpenCameraEvent(cameraManager.getVidPid());

  if (autoTake) {
    const takePromise = cameraView.beginTake(
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
}

// This is the entry point of CCA so the returned promise is not awaited.
void main();
