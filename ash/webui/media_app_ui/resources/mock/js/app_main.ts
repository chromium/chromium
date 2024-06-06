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
 */
type ModuleHandler = (blobSrc: string, file: AbstractFile) =>
    Promise<HTMLElement>;

const createVideoChild: ModuleHandler = async (blobSrc) => {
  const video = document.createElement('video');
  video.src = blobSrc;
  return video;
};

const createImgChild: ModuleHandler = async (blobSrc, file) => {
  const img = document.createElement('img');
  img.src = blobSrc;
  img.alt = file.name;
  try {
    await img.decode();
  } catch (error) {
    // Mimic what the real app does on decode errors so we can test error
    // handling for file access.
    return await createErrorChild(blobSrc, file);
  }
  return img;
};

const createAudioChild: ModuleHandler = async (blobSrc, file) => {
  const container = document.createElement('div');

  const title = container.appendChild(document.createElement('div'));
  title.className = 'title';
  title.innerText = file.name;

  const audio = container.appendChild(document.createElement('audio'));
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

const createPdfChild: ModuleHandler = async (_, file) => {
  const container = document.createElement('div');

  if (file.size === 0) {
    console.warn('Assuming zero-byte test file: not loading PDF.');
    return container;
  }

  container.appendChild(document.createElement('canvas'));
  return container;
};

const createErrorChild: ModuleHandler = async (_, file) => {
  console.warn(`Mock handling of ${file.name} resulted in error.`);
  // In the real app, a loaderror element is loaded infront of a placeholder
  // image with some error alt text. For tests, we only mock the placeholder.
  const img = document.createElement('img');
  img.alt = 'Unable to decode';
  return img;
};

/**
 * A mock app used for testing when src-internal is not available.
 */
class BacklightApp extends HTMLElement implements ClientApi {
  appBar: BacklightAppBar;
  currentHandler: HTMLElement;
  currentMedia: HTMLElement;

  files?: AbstractFileList;
  delegate?: ClientApiDelegate|null;

  constructor() {
    super();
    const shadowRoot = this.attachShadow({mode: 'open'});
    this.appBar =
        document.createElement('backlight-app-bar') as BacklightAppBar;

    shadowRoot.appendChild(this.appBar);
    this.currentHandler = document.createElement('backlight-media-handler');
    this.appendChild(this.currentHandler);
    this.currentMedia = document.createElement('img');
    this.appendChild(this.currentMedia);
  }

  /**
   * Emulates the preprocessing done in the "real" BacklightApp to hook in the
   * RAW file converter. See go/media-app-element.
   */
  private async preprocessFile(file: AbstractFile|null) {
    // This mock is only used for tests (which only test a .orf and .nef RAW
    // file). We don't maintain the full list of RAW extensions here.
    const rawExtensions = ['orf', 'nef'];
    const isRawFile =
        file && rawExtensions.includes(file.name.split('.').pop() ?? '');
    if (isRawFile) {
      file.blob = await this.delegate!.extractPreview(file.blob);
      file.mimeType = 'image/x-RAW';
    }
  }

  /**
   * Emulates the sniffing done in the "real" BacklightApp to detect a file's
   * mime type. See go/media-app-element.
   *
   * @return The sniffed mime type, or empty string if none detected.
   */
  private async sniffedMimeType(file: AbstractFile): Promise<string> {
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
   */
  async loadFile(file: AbstractFile) {
    await this.preprocessFile(file);
    const mimeType = file.mimeType || await this.sniffedMimeType(file);
    let factory;
    const [type, subtype] = mimeType.split('/');
    switch (type) {
      case 'video':
        factory = createVideoChild;
        break;
      case 'image':
        factory = createImgChild;
        break;
      case 'audio':
        factory = createAudioChild;
        break;
      case 'application':
        if (subtype === 'pdf') {
          factory = createPdfChild;
          break;
        }
        factory = createErrorChild;
        break;
      default:
        factory = createErrorChild;
    }
    // Note the mock app will just leak this Blob URL.
    const child = await factory(URL.createObjectURL(file.blob), file);

    this.replaceChild(child, this.currentMedia);
    this.currentMedia = child;
    if (mimeType === 'application/pdf') {
      this.delegate!.notifyFileOpened(file.name, mimeType);
    }
    this.delegate!.notifyCurrentFile(file.name, mimeType);
    this.appBar.setFilename(file.name);
  }

  updateHandler() {
    // Loads a new handler each time a new media is loaded. Note: in actual
    // implementation we cache our handler instances and early exit if we load
    // the same media type.
    const newHandler = document.createElement('backlight-media-handler');
    this.replaceChild(newHandler, this.currentHandler);
    this.currentHandler = newHandler;

    // The presence of the 'filetraversalenabled' attribute emulates
    // `setFileTraversalEnabled()` in the real app.
    this.currentHandler.toggleAttribute(
        'filetraversalenabled', this.files!.length > 1);
  }

  // ClientApi implementation:
  async loadFiles(files: AbstractFileList) {
    this.files = files;
    files.addObserver((f: AbstractFileList) => this.onNewFiles(f));

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

  async requestBitmap(_requestedPageId: string) {
    return {
      'page': {
        'imageInfo': {
          'alphaType': 1,
          'width': 1,
          'height': 1,
          'colorTransferFunction': null,
          'colorToXyzMatrix': null,
        },
        'pixelData': {
          'bytes': [],
          'sharedMemory': undefined,
          'invalidBuffer': undefined,
        },
      },
    };
  }

  async setViewport(_viewport: RectF) {}

  async setPdfOcrEnabled(_enabled: boolean) {}

  async getPdfContent(_limit: number) {
    return {
      'content': 'test content',
    };
  }

  async hidePdfContextMenu() {}

  setDelegate(delegate: ClientApiDelegate|null) {
    this.delegate = delegate;
  }

  onNewFiles(files: AbstractFileList) {
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

// Element mimicking the app bar which contains the current file's name.
class BacklightAppBar extends HTMLElement {
  child: HTMLElement;

  constructor() {
    super();
    const shadowRoot = this.attachShadow({mode: 'open'});
    this.child = document.createElement('div');
    this.child.classList.add('app-bar-filename');

    shadowRoot.appendChild(this.child);
  }

  setFilename(filename: string) {
    this.child.setAttribute('filename', filename);
  }
}
window.customElements.define('backlight-app-bar', BacklightAppBar);

// Element mimicking the image/video handler which is the parent of the
// `carousel-overlay`.
class BacklightMediaHandler extends HTMLElement {}
window.customElements.define('backlight-media-handler', BacklightMediaHandler);

class VideoContainer extends HTMLElement {}
window.customElements.define('backlight-video-container', VideoContainer);

// Add error handlers similar to go/error-collector to test crash reporting in
// the mock app. The handlers in go/error-collector are compiled-in and have
// more sophisticated message extraction, with fewer assumptions.
function sendCrashReport(params: any) {
  (window as any).chrome.crashReportPrivate.reportError(params, () => {});
}
self.addEventListener('error', (errorEvent: ErrorEvent) => {
  sendCrashReport({
    message: errorEvent.error.message,
    url: window.location.href,
    product: 'ChromeOS_MediaAppMock',
    lineNumber: errorEvent.lineno,
    columnNumber: errorEvent.colno,
  });
});
self.addEventListener('unhandledrejection', (event: Event) => {
  const rejectionEvent = event as unknown as {reason: Error};
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
  return undefined;
});
