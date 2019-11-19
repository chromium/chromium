// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Responsible for loading scripts into the inject context.
 */

goog.provide('InjectedScriptLoader');



/** @constructor */
InjectedScriptLoader = function() {};


/**
 * Loads a dictionary of file contents for Javascript files.
 * @param {Array<string>} files A list of file names.
 * @param {function(Object<string,string>)} done A function called when all
 *     the files have been loaded. Called with the code map as the first
 *     parameter.
 */
InjectedScriptLoader.fetchCode = function(files, done) {
  var code = {};
  var waiting = files.length;
  var startTime = new Date();
  var loadScriptAsCode = function(src) {
    // Load the script by fetching its source and running 'eval' on it
    // directly, with a magic comment that makes Chrome treat it like it
    // loaded normally. Wait until it's fetched before loading the
    // next script.
    var xhr = new XMLHttpRequest();
    var url = chrome.extension.getURL(src) + '?' + new Date().getTime();
    xhr.onreadystatechange = function() {
      if (xhr.readyState == 4) {
        var scriptText = xhr.responseText;
        // Add a magic comment to the bottom of the file so that
        // Chrome knows the name of the script in the JavaScript debugger.
        var debugSrc = src.replace('closure/../', '');
        // The 'chromevox' id is only used in the DevTools instead of a long
        // extension id.
        scriptText +=
            '\n//# sourceURL= chrome-extension://chromevox/' + debugSrc + '\n';
        code[src] = scriptText;
        waiting--;
        if (waiting == 0) {
          done(code);
        }
      }
    };
    xhr.open('GET', url);
    xhr.send(null);
  };

  files.forEach(function(f) {
    loadScriptAsCode(f);
  });
};
