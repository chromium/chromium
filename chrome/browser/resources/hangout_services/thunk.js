// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.runtime.onMessageExternal.addListener(function(
    message, sender, sendResponse) {
  function doSendResponse(value, errorString) {
    let error = null;
    if (errorString) {
      error = {};
      error['name'] = 'ComponentExtensionError';
      error['message'] = errorString;
    }

    const errorMessage = error || chrome.runtime.lastError;
    sendResponse({'value': value, 'error': errorMessage});
  }

  function getHost(url) {
    if (!url) {
      return '';
    }
    const origin = new URL(url).origin;
    return `${origin}/`;
  }

  try {
    const requestInfo = {};

    // Set the tab ID. If it's passed in the message, use that.
    // Otherwise use the sender information.
    if (message['tabId']) {
      requestInfo['tabId'] = +message['tabId'];
      if (isNaN(requestInfo['tabId'])) {
        throw new Error(
            'Cannot convert tab ID string to integer: ' + message['tabId']);
      }
    } else if (sender.tab) {
      requestInfo['tabId'] = sender.tab.id;
    }

    if (sender.guestProcessId) {
      requestInfo['guestProcessId'] = sender.guestProcessId;
    }

    const method = message['method'];

    // Set the origin. If a URL is passed in the message, use that.
    // Otherwise use the sender information.
    let origin;
    if (message['winUrl']) {
      origin = getHost(message['winUrl']);
    } else {
      origin = getHost(sender.url);
    }

    if (method === 'cpu.getInfo') {
      chrome.system.cpu.getInfo(doSendResponse);
      return true;
    } else if (method === 'logging.setMetadata') {
      const metaData = message['metaData'];
      chrome.webrtcLoggingPrivate.setMetaData(
          requestInfo, origin, metaData, doSendResponse);
      return true;
    } else if (method === 'logging.start') {
      chrome.webrtcLoggingPrivate.start(requestInfo, origin, doSendResponse);
      return true;
    } else if (method === 'logging.uploadOnRenderClose') {
      chrome.webrtcLoggingPrivate.setUploadOnRenderClose(
          requestInfo, origin, true);
      doSendResponse();
      return false;
    } else if (method === 'logging.stop') {
      chrome.webrtcLoggingPrivate.stop(requestInfo, origin, doSendResponse);
      return true;
    } else if (method === 'logging.stopAndUpload') {
      // Stop everything and upload. This is allowed to be called even if
      // logs have already been stopped or not started. Therefore, ignore
      // any errors along the way, but store them, so that if upload fails
      // they are all reported back.
      const errors = [];
      chrome.webrtcLoggingPrivate.stop(requestInfo, origin, function() {
        appendLastErrorMessage(errors);
        chrome.webrtcLoggingPrivate.upload(
            requestInfo, origin, function(uploadValue) {
              let errorMessage = null;
              // If upload fails, report all previous errors.
              // Otherwise, throw them away.
              if (chrome.runtime.lastError !== undefined) {
                appendLastErrorMessage(errors);
                errorMessage = errors.join('; ');
              }
              doSendResponse(uploadValue, errorMessage);
            });
      });
      return true;
    } else if (method === 'logging.store') {
      const logId = message['logId'];
      chrome.webrtcLoggingPrivate.store(
          requestInfo, origin, logId, doSendResponse);
      return true;
    } else if (method === 'logging.discard') {
      chrome.webrtcLoggingPrivate.discard(requestInfo, origin, doSendResponse);
      return true;
    } else if (method === 'logging.startEventLogging') {
      const sessionId = message['sessionId'] || '';
      const maxLogSizeBytes = message['maxLogSizeBytes'] || 0;
      const outputPeriodMs = message['outputPeriodMs'] || -1;
      const webAppId = message['webAppId'] || 0;
      chrome.webrtcLoggingPrivate.startEventLogging(
          requestInfo, origin, sessionId, maxLogSizeBytes, outputPeriodMs,
          webAppId, doSendResponse);
      return true;
    } else if (method === 'getHardwarePlatformInfo') {
      chrome.enterprise.hardwarePlatform.getHardwarePlatformInfo(
          doSendResponse);
      return true;
    }

    throw new Error('Unknown method: ' + method);
  } catch (e) {
    doSendResponse(null, e.name + ': ' + e.message);
  }
});

// This is a one-time-use port for calling chooseDesktopMedia.  The page
// sends one message, identifying the requested source types, and the
// extension sends a single reply, with the user's selected streamId.  A port
// is used so that if the page is closed before that message is sent, the
// window picker dialog will be closed.
function onChooseDesktopMediaPort(port) {
  function sendResponse(streamId) {
    port.postMessage({'value': {'streamId': streamId}});
    port.disconnect();
  }

  port.onMessage.addListener(function(message) {
    const method = message['method'];
    if (method === 'chooseDesktopMedia') {
      const sources = message['sources'];

      // Options that getDisplayMedia() also has, which are applied *before*
      // the user makes their choice, and which typically serve to shape
      // the choice offered to the user.
      const options = message['options'] ||
          {systemAudio: 'include', selfBrowserSurface: 'include'};

      // For historical reasons, default to the common behavior of users
      // of the Hangouts Extension, but still allow users of this API
      // to explicitly set their own value.
      if (!('suppressLocalAudioPlaybackIntended' in options)) {
        options['suppressLocalAudioPlaybackIntended'] = true;
      }

      let cancelId = null;
      const tab = port.sender.tab;
      if (tab) {
        // Per crbug.com/425344, in order to allow an <iframe> on a different
        // domain, to get desktop media, we need to set the tab.url to match
        // the <iframe>, even though it doesn't really load the new url.
        tab.url = port.sender.url;
        cancelId = chrome.desktopCapture.chooseDesktopMedia(
            sources, tab, options, sendResponse);
      } else {
        const requestInfo = {};
        requestInfo['guestProcessId'] = port.sender.guestProcessId || 0;
        requestInfo['guestRenderFrameId'] =
            port.sender.guestRenderFrameRoutingId || 0;
        // TODO(crbug.com/1329129): Plumb systemAudio, selfBrowserSurface and
        // other options here.
        cancelId = chrome.webrtcDesktopCapturePrivate.chooseDesktopMedia(
            sources, requestInfo, sendResponse);
      }
      port.onDisconnect.addListener(function() {
        // This method has no effect if called after the user has selected a
        // desktop media source, so it does not need to be conditional.
        if (tab) {
          chrome.desktopCapture.cancelChooseDesktopMedia(cancelId);
        } else {
          chrome.webrtcDesktopCapturePrivate.cancelChooseDesktopMedia(cancelId);
        }
      });
    }
  });
}

// A port for continuously reporting relevant CPU usage information to the page.
function onProcessCpu(port) {
  let tabPid = port.sender.guestProcessId || undefined;
  function processListener(processes) {
    if (tabPid === undefined) {
      // getProcessIdForTab sometimes fails, and does not call the callback.
      // (Tracked at https://crbug.com/368855.)
      // This call retries it on each process update until it succeeds.
      chrome.processes.getProcessIdForTab(port.sender.tab.id, function(x) {
        tabPid = x;
      });
      return;
    }
    const tabProcess = processes[tabPid];
    if (!tabProcess) {
      return;
    }

    let browserProcessCpu;
    let gpuProcessCpu;
    for (const pid in processes) {
      const process = processes[pid];
      if (process.type === 'browser') {
        browserProcessCpu = process.cpu;
      } else if (process.type === 'gpu') {
        gpuProcessCpu = process.cpu;
      }
      if (browserProcessCpu && gpuProcessCpu) {
        break;
      }
    }

    port.postMessage({
      'browserCpuUsage': browserProcessCpu || 0,
      'gpuCpuUsage': gpuProcessCpu || 0,
      'tabCpuUsage': tabProcess.cpu,
      'tabJsMemoryAllocated': tabProcess.jsMemoryAllocated,
      'tabJsMemoryUsed': tabProcess.jsMemoryUsed,
    });
  }

  chrome.processes.onUpdated.addListener(processListener);
  port.onDisconnect.addListener(function() {
    chrome.processes.onUpdated.removeListener(processListener);
  });
}

function appendLastErrorMessage(errors) {
  if (chrome.runtime.lastError !== undefined) {
    errors.push(chrome.runtime.lastError.message);
  }
}

chrome.runtime.onConnectExternal.addListener(function(port) {
  if (port.name === 'chooseDesktopMedia') {
    onChooseDesktopMediaPort(port);
  } else if (port.name === 'processCpu') {
    onProcessCpu(port);
  } else {
    // Unknown port type.
    port.disconnect();
  }
});
