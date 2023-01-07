// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Defines the ScriptInstaller functions which install scripts
 * into the web page (not a content script)
 *
 */

goog.provide('ScriptInstaller');


/**
 * URL pattern where we do not allow script installation.
 * @type {RegExp}
 */
ScriptInstaller.denylistPattern = /chrome:\/\/|chrome-extension:\/\//;

/**
 * Installs a script in the web page.
 * @param {string} src A URL for a script.
 * @param {string} uid A unique id.  This function won't install the same set of
 *      scripts twice.
 * @return {boolean} False if the script already existed and this function
 * didn't do anything.
 */
ScriptInstaller.installScript = function(src, uid) {
  if (ScriptInstaller.denylistPattern.test(document.URL)) {
    return false;
  }
  if (document.querySelector('script[' + uid + ']')) {
    ScriptInstaller.uninstallScript(uid);
  }
  if (!src) {
    return false;
  }

  ScriptInstaller.installScriptHelper_(src, uid);
  return true;
};

/**
 * Uninstalls a script.
 * @param {string} uid Id of the script node.
 */
ScriptInstaller.uninstallScript = function(uid) {
  let scriptNode;
  if (scriptNode = document.querySelector('script[' + uid + ']')) {
    scriptNode.remove();
  }
};

/**
 * Helper that installs one script and calls itself recursively when each
 * script loads.
 * @param {string} src A URL for a script.
 * @param {string} uid A unique id.  This function won't install the same set of
 *      scripts twice.
 * @private
 */
ScriptInstaller.installScriptHelper_ = function(src, uid) {
  if (!src) {
    return;
  }

  const xhr = new XMLHttpRequest();
  const url = src + '?' + new Date().getTime();
  xhr.onreadystatechange = function() {
    if (xhr.readyState === 4) {
      const scriptText = xhr.responseText;
      const apiScript = document.createElement('script');
      apiScript.type = 'text/javascript';
      apiScript.setAttribute(uid, '1');
      apiScript.textContent = scriptText;
      const scriptOwner = document.head || document.body;
      scriptOwner.appendChild(apiScript);
    }
  };

  try {
    xhr.open('GET', url, true);
    xhr.send(null);
  } catch (exception) {
    console.log(
        'Warning: ChromeVox external script loading for ' + document.location +
        ' stopped after failing to install ' + src);
  }
};
