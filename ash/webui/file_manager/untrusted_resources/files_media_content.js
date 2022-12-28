// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

window.onload = () => {
  let messageSource;
  let filesAppOrigin;

  const content = document.querySelector('#content');
  let contentUrl;

  /**
   * Identifies every message to load an content. Used to check if the current
   * load is still valid after an async operation.
   *
   * @type {number}
   */
  let loadId = 0;

  window.addEventListener('message', event => {
    if (event.origin !== FILES_APP_SWA_ORIGIN) {
      console.error('Unknown origin: ' + event.origin);
      return;
    }

    const currentLoadId = ++loadId;
    function isValidLoad() {
      return currentLoadId === loadId;
    }

    // Release Object URLs generated with URL.createObjectURL.
    URL.revokeObjectURL(contentUrl);
    contentUrl = '';

    filesAppOrigin = event.origin;
    messageSource = event.source;

    /** @type {!UntrustedPreviewData} */
    const data = event.data;

    const sourceContent = data.sourceContent;
    switch (sourceContent.dataType) {
      case 'url':
        contentUrl = /** @type {string} */ (sourceContent.data);
        break;
      case 'blob':
        contentUrl =
            URL.createObjectURL(/** @type {!Blob} */ (sourceContent.data));
        break;
      default:
        contentUrl = '';
    }

    switch (data.type) {
      case 'html':
        content.textContent = '';
        contentChanged(null);
        fetch(contentUrl)
            .then((response) => {
              if (!isValidLoad()) {
                return;
              }
              return response.text();
            })
            .then((text) => {
              if (!isValidLoad()) {
                return;
              }
              content.textContent = text;
              contentChanged(text);
            });
        break;
      case 'audio':
      case 'video':
        content.onloadeddata = (e) => {
          if (!isValidLoad()) {
            return;
          }
          contentChanged(e.target.src);
        };
        content.src = contentUrl;
        break;
      case 'image':
        content.remove();
        content.src = '';

        const image = new Image();
        image.onload = (e) => {
          document.body.appendChild(content);
          if (!isValidLoad()) {
            return;
          }
          contentChanged(e.target.src);
          content.src = e.target.src;
        };

        image.onerror = (e) => {
          contentDecodeFailed();
        };

        image.src = contentUrl;
        break;
      default:
        content.onload = (e) => isValidLoad() && contentChanged(e.target.src);
        content.src = contentUrl;
        break;
    }
  });

  document.addEventListener('contextmenu', e => {
    e.preventDefault();
    return false;
  });

  document.addEventListener('click', e => {
    sendMessage((e.target === content) ? 'tap-inside' : 'tap-outside');
  });

  function contentChanged(src) {
    sendMessage(src ? 'webview-loaded' : 'webview-cleared');
  }

  function contentDecodeFailed() {
    sendMessage('content-decode-failed');
  }

  function sendMessage(message) {
    if (messageSource) {
      messageSource.postMessage(message, filesAppOrigin);
    }
  }

  // TODO(oka): This is a workaround to fix FOUC problem, where sometimes
  // immature view with smaller window size than outer window is rendered for a
  // moment. Remove this after the root cause is fixed. http://crbug.com/640525
  window.addEventListener('resize', () => {
    // Remove hidden attribute on event of resize to avoid FOUC. The window's
    // initial size is 100 x 100 and it's fit into the outer window size after a
    // moment. Due to Files App's window size constraint, resized window must be
    // larger than 100 x 100. So this event is always invoked.
    content.removeAttribute('hidden');
  });
  // Fallback for the case of webview bug is fixed and above code is not
  // executed.
  setTimeout(() => {
    content.removeAttribute('hidden');
  }, 500);
};

// clang-format off
//# sourceURL=//ash/webui/file_manager/untrusted_resources/files_media_content.js
