// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Queue of pending requests from an origin.
 *
 */
'use strict';

/**
 * Represents a queued request. Once given a token, call complete() once the
 * request is processed (or dropped.)
 * @interface
 */
function QueuedRequestToken() {}

/** Completes (or cancels) this queued request. */
QueuedRequestToken.prototype.complete = function() {};

/**
 * @param {!RequestQueue} queue The queue for this request.
 * @param {number} id An id for this request.
 * @param {function(QueuedRequestToken)} beginCb Called when work may begin on
 *     this request.
 * @param {RequestToken} opt_prev Previous request in the same queue.
 * @param {RequestToken} opt_next Next request in the same queue.
 * @constructor
 * @implements {QueuedRequestToken}
 */
function RequestToken(queue, id, beginCb, opt_prev, opt_next) {
  /** @private {!RequestQueue} */
  this.queue_ = queue;
  /** @private {number} */
  this.id_ = id;
  /** @private {boolean} */
  this.begun_ = false;
  /** @private {function(QueuedRequestToken)} */
  this.beginCb_ = beginCb;
  /** @type {RequestToken} */
  this.prev = null;
  /** @type {RequestToken} */
  this.next = null;
  /** @private {boolean} */
  this.completed_ = false;
}

/** Begins work on this queued request. */
RequestToken.prototype.begin = function() {
  this.begun_ = true;
  this.beginCb_(this);
};

/** @return {boolean} Whether this token has already begun. */
RequestToken.prototype.begun = function() {
  return this.begun_;
};

/** Completes (or cancels) this queued request. */
RequestToken.prototype.complete = function() {
  if (this.completed_) {
    // Either the caller called us more than once, or the timer is firing.
    // Either way, nothing more to do here.
    return;
  }
  this.completed_ = true;
  this.queue_.complete(this);
};

/** @return {boolean} Whether this token has already completed. */
RequestToken.prototype.completed = function() {
  return this.completed_;
};

/** @return {number} This token's id. */
RequestToken.prototype.id = function() {
  return this.id_;
};


/**
 * @param {!SystemTimer} sysTimer A system timer implementation.
 * @constructor
 */
function RequestQueue(sysTimer) {
  /** @private {!SystemTimer} */
  this.sysTimer_ = sysTimer;
  /** @private {RequestToken} */
  this.head_ = null;
  /** @private {RequestToken} */
  this.tail_ = null;
  /** @private {number} */
  this.id_ = 0;
}

/**
 * Inserts this token into the queue.
 * @param {RequestToken} token Queue token
 * @private
 */
RequestQueue.prototype.insertToken_ = function(token) {
  console.info(UTIL_fmt('token ' + this.id_ + ' inserted'));
  if (this.head_ === null) {
    this.head_ = token;
    this.tail_ = token;
  } else {
    if (!this.tail_) {
      throw new Error('Non-empty list missing tail');
    }
    this.tail_.next = token;
    token.prev = this.tail_;
    this.tail_ = token;
  }
};

/**
 * Removes this token from the queue.
 * @param {RequestToken} token Queue token
 * @return {RequestToken?} The next token in the queue to run, if any.
 * @private
 */
RequestQueue.prototype.removeToken_ = function(token) {
  var nextTokenToRun = null;
  // If this token has been begun, find the next token to run.
  if (token.begun()) {
    // Find the first token in the queue which has not yet been begun, and which
    // is not the token being removed.
    for (var nextToken = this.head_; nextToken; nextToken = nextToken.next) {
      if (nextToken !== token && !nextToken.begun()) {
        nextTokenToRun = nextToken;
        break;
      }
    }
  }

  // Remove this token from the queue
  if (token.next) {
    token.next.prev = token.prev;
  }
  if (token.prev) {
    token.prev.next = token.next;
  }

  // Update head and tail of queue.
  if (this.head_ === token && this.tail_ === token) {
    this.head_ = this.tail_ = null;
  } else {
    if (this.head_ === token) {
      this.head_ = token.next;
      this.head_.prev = null;
    }
    if (this.tail_ === token) {
      this.tail_ = token.prev;
      this.tail_.next = null;
    }
  }

  // Isolate this token to prevent it from manipulating the queue, e.g. if
  // complete() is called a second time with it.
  token.prev = token.next = null;

  return nextTokenToRun;
};

/**
 * Completes this token's request, and begins the next queued request, if one
 * exists.
 * @param {RequestToken} token Queue token
 */
RequestQueue.prototype.complete = function(token) {
  var next = this.removeToken_(token);
  if (next) {
    console.info(
        UTIL_fmt('token ' + token.id() + ' completed, starting ' + next.id()));
    next.begin();
  } else if (this.empty()) {
    console.info(UTIL_fmt('token ' + token.id() + ' completed, queue empty'));
  } else {
    console.info(UTIL_fmt(
        'token ' + token.id() + ' completed (earlier token still running)'));
  }
};

/** @return {boolean} Whether this queue is empty. */
RequestQueue.prototype.empty = function() {
  return this.head_ === null;
};

/**
 * Queues this request, and, if it's the first request, begins work on it.
 * @param {function(QueuedRequestToken)} beginCb Called when work begins on this
 *     request.
 * @param {Countdown} timer Countdown timer
 * @return {QueuedRequestToken} A token for the request.
 */
RequestQueue.prototype.queueRequest = function(beginCb, timer) {
  var startNow = this.empty();
  var token = new RequestToken(this, ++this.id_, beginCb);
  // Clone the timer to set a callback on it, which will ensure complete() is
  // eventually called, even if the caller never gets around to it.
  timer.clone(token.complete.bind(token));
  this.insertToken_(token);
  if (startNow) {
    this.sysTimer_.setTimeout(function() {
      if (!token.completed()) {
        token.begin();
      }
    }, 0);
  }
  return token;
};

/**
 * @param {!SystemTimer} sysTimer A system timer implementation.
 * @constructor
 */
function OriginKeyedRequestQueue(sysTimer) {
  /** @private {!SystemTimer} */
  this.sysTimer_ = sysTimer;
  /** @private {Object<string, !RequestQueue>} */
  this.requests_ = {};
}

/**
 * Queues this request, and, if it's the first request, begins work on it.
 * @param {string} appId Application Id
 * @param {string} origin Request origin
 * @param {function(QueuedRequestToken)} beginCb Called when work begins on this
 *     request.
 * @param {Countdown} timer Countdown timer
 * @return {QueuedRequestToken} A token for the request.
 */
OriginKeyedRequestQueue.prototype.queueRequest = function(
    appId, origin, beginCb, timer) {
  var key = appId + ' ' + origin;
  if (!this.requests_.hasOwnProperty(key)) {
    this.requests_[key] = new RequestQueue(this.sysTimer_);
  }
  var queue = this.requests_[key];
  return queue.queueRequest(beginCb, timer);
};
