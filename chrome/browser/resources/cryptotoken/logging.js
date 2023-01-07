// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Logging related utility functions.
 */

/** Posts the log message to the log url.
 * @param {string} logMsg the log message to post.
 * @param {string=} opt_logMsgUrl the url to post log messages to.
 */
function logMessage(logMsg, opt_logMsgUrl) {
  console.warn(UTIL_fmt('logMessage("' + logMsg + '")'));

  if (!opt_logMsgUrl) {
    return;
  }

  var audio = new Audio();
  audio.src = opt_logMsgUrl + logMsg;
}
