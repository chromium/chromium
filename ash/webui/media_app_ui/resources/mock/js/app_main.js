// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * Provides a mock of http://go/media-app/index.ts which is pre-built and
 * brought in via DEPS to ../../app/js/app_main.js. Runs in an isolated guest.
 */

/**
 * Helper that returns UI that can serve as an effective mock of a fragment of
 * the real app, after loading a provided Blob URL.
 *
 * @typedef{function(string, mediaApp.AbstractFile): Promise<!HTMLElement>}}
 */
let ModuleHandler;

/** @type {ModuleHandler} */
const createVideoChild = async (blobSrc) => {
  const video =
      /** @type {HTMLVideoElement} */ (document.createElement('video'));
  video.src = blobSrc;
  return video;
};

/** @type {ModuleHandler} */
const createImgChild = async (blobSrc, file) => {
  const img = /** @type {!HTMLImageElement} */ (document.createElement('img'));
  img.src = blobSrc;
  img.alt = file.name;
  try {
    await img.decode();
  } catch (error) {
    // Mimic what the real app does on decode errors so we can test error
    // handling for file access.
    return /** @type {!HTMLElement} */ (await createErrorChild(blobSrc, file));
  }
  return img;
};

/** @type {ModuleHandler} */
const createAudioChild = async (blobSrc, file) => {
  const container =
      /** @type {HTMLDivElement} */ (document.createElement('div'));

  const title = /** @type {HTMLDivElement} */
      (container.appendChild(document.createElement('div')));
  title.className = 'title';
  title.innerText = file.name;

  const audio = /** @type {HTMLAudioElement} */
      (container.appendChild(document.createElement('audio')));
  if (file.size === 0) {
    console.warn('Assuming zero-byte test file: not loading audio.');
    return container;
  }

  audio.src = blobSrc;
  // Audio will autoplay in this manner. Do the same in the mock to test
  // integration points.
  audio.play()
      .then(() => {
        console.log('Audio playing..');
      })
      .catch(e => {
        console.error(String(e));
      });
  return container;
};

/** @type {ModuleHandler} */
const createErrorChild = async (_, file) => {
  console.warn(`Mock handling of ${file.name} resulted in error.`);
  // In the real app, a loaderror element is loaded infront of a placeholder
  // image with some error alt text. For tests, we only mock the placeholder.
  const img = /** @type {!HTMLImageElement} */ (document.createElement('img'));
  img.alt = 'Unable to decode';
  return img;
};

/**
 * A mock app used for testing when src-internal is not available.
 * @implements mediaApp.ClientApi
 */
class BacklightApp extends HTMLElement {
  constructor() {
    super();
    /** @type {?HTMLElement} */
    this.currentHandler = /** @type {HTMLElement} */ (
        document.createElement('backlight-media-handler'));
    this.appendChild(this.currentHandler);
    this.currentMedia =
        /** @type {!HTMLElement} */ (document.createElement('img'));
    this.appendChild(this.currentMedia);
    /** @type {?mediaApp.AbstractFileList} */
    this.files;
    /** @type {?mediaApp.ClientApiDelegate} */
    this.delegate;
  }

  /**
   * Emulates the preprocessing done in the "real" BacklightApp to hook in the
   * RAW file converter. See go/media-app-element.
   *
   * @param {?mediaApp.AbstractFile} file
   * @private
   */
  async preprocessFile(file) {
    // This mock is only used for tests (which only test a .orf and .nef RAW
    // file). We don't maintain the full list of RAW extensions here.
    const rawExtensions = ['orf', 'nef'];
    const isRawFile =
        file && rawExtensions.includes(file.name.split('.').pop());
    if (isRawFile) {
      file.blob = await this.delegate.extractPreview(file.blob);
      file.mimeType = 'image/x-RAW';
    }
  }

  /**
   * Emulates the sniffing done in the "real" BacklightApp to detect a file's
   * mime type. See go/media-app-element.
   *
   * @param {?mediaApp.AbstractFile} file
   * @return {!Promise<string>} The sniffed mime type, or empty string if none
   *     detected.
   * @private
   */
  async sniffedMimeType(file) {
    const START_OF_IMAGE_MARKER = 0xffd8;

    const PNG_MARKER = 0x89504e47;  // Literally '‰PNG'.

    if (file.size < 4) {
      return '';
    }

    const view = new DataView(await file.blob.slice(0, 4).arrayBuffer());
    if (view.getUint32(0) === PNG_MARKER) {
      return 'image/png';
    }
    if (view.getUint16(0) === START_OF_IMAGE_MARKER) {
      return 'image/jpeg';
    }
    return '';
  }

  /**
   * Emulates loading a single file at a time.
   * @param {!mediaApp.AbstractFile} file
   */
  async loadFile(file) {
    await this.preprocessFile(file);
    const mimeType = file.mimeType || await this.sniffedMimeType(file);
    let factory;
    switch (mimeType.split('/')[0]) {
      case 'video':
        factory = createVideoChild;
        break;
      case 'image':
        factory = createImgChild;
        break;
      case 'audio':
        factory = createAudioChild;
        break;
      default:
        factory = createErrorChild;
    }
    // Note the mock app will just leak this Blob URL.
    const child = await factory(URL.createObjectURL(file.blob), file);

    this.replaceChild(child, this.currentMedia);
    this.currentMedia = child;
    this.delegate.notifyCurrentFile(file.name, mimeType);
  }

  updateHandler() {
    // Loads a new handler each time a new media is loaded. Note: in actual
    // implementation we cache our handler instances and early exit if we load
    // the same media type.
    const newHandler = /** @type {HTMLElement} */ (
        document.createElement('backlight-media-handler'));
    this.replaceChild(newHandler, this.currentHandler);
    this.currentHandler = newHandler;

    // The presence of the 'filetraversalenabled' attribute emulates
    // `setFileTraversalEnabled()` in the real app.
    this.currentHandler.toggleAttribute(
        'filetraversalenabled', this.files.length > 1);
  }

  /** @override  */
  async loadFiles(files) {
    this.files = files;
    files.addObserver((f) => this.onNewFiles(f));

    const file = files.item(files.currentFileIndex);
    if (!file) {
      // Emulate zero state.
      const child = document.createElement('img');
      this.replaceChild(child, this.currentMedia);
      this.currentMedia = child;
    } else {
      this.loadFile(file);
    }
    this.updateHandler();
  }

  /** @override */
  setDelegate(delegate) {
    this.delegate = delegate;
  }

  /** @param {!mediaApp.AbstractFileList} files */
  onNewFiles(files) {
    if (files !== this.files) {
      return;
    }

    // Handlers in the real app contain a notion of the "current" file. For the
    // mock, assume here that the current file does not change when new files
    // are added to the file list. However, we still "update" the handler to
    // ensure it reflects the navigation state when new files are added. Note
    // merely toggling attributes here will fail to notify mutation observers.
    this.updateHandler();
  }
}

window.customElements.define('backlight-app', BacklightApp);

// Element mimicking the image/video handler which is the parent of the
// `carousel-overlay`.
class BacklightMediaHandler extends HTMLElement {}
window.customElements.define('backlight-media-handler', BacklightMediaHandler);

class VideoContainer extends HTMLElement {}
window.customElements.define('backlight-video-container', VideoContainer);

// Add error handlers similar to go/error-collector to test crash reporting in
// the mock app. The handlers in go/error-collector are compiled-in and have
// more sophisticated message extraction, with fewer assumptions.

/** @suppress{reportUnknownTypes,missingProperties} */
function sendCrashReport(params) {
  chrome.crashReportPrivate.reportError(params, () => {});
}
self.addEventListener('error', (event) => {
  const errorEvent = /** @type {ErrorEvent} */ (event);
  sendCrashReport({
    message: /** @type {Error} */ (errorEvent.error).message,
    url: window.location.href,
    product: 'ChromeOS_MediaAppMock',
    lineNumber: errorEvent.lineno,
    columnNumber: errorEvent.colno,
  });
});
self.addEventListener('unhandledrejection', (event) => {
  const rejectionEvent = /** @type {{reason: Error}} */ (event);
  sendCrashReport({
    message: rejectionEvent.reason.message,
    url: window.location.href,
    product: 'ChromeOS_MediaAppMock',
  });
});

document.addEventListener('DOMContentLoaded', () => {
  // The "real" app first loads translations for populating strings in the app
  // for the initial load, then does this. See go/media-app-loadapp.
  const app = new BacklightApp();
  document.body.appendChild(app);

  if (window.customLaunchData) {
    if (window.customLaunchData.delegate) {
      app.setDelegate(window.customLaunchData.delegate);
    }
    return app.loadFiles(window.customLaunchData.files);
  }
});
