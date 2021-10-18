// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @typedef {{
 *   initGA: function(string, string, function(string): void): !Promise,
 *   sendGAEvent: function(!UniversalAnalytics.FieldsObject): !Promise,
 *   setMetricsEnabled: function(string, boolean): !Promise,
 * }}
 */
export let GAHelperInterface;

/**
 * @typedef {{
 *   connectToWorker: function(!MessagePort): !Promise,
 * }}
 */
export let VideoProcessorHelperInterface;
