// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview imports javascript files that have side effects. Makes sure
 * they are imported in the correct order.
 */

// Import necessary built in modules before ../common files. This will
// guarantee that polymer and certain polymer elements are loaded first.
import 'chrome-untrusted://personalization/polymer/v3_0/iron-icon/iron-icon.js';
import 'chrome-untrusted://personalization/polymer/v3_0/iron-iconset-svg/iron-iconset-svg.js';
import 'chrome-untrusted://personalization/polymer/v3_0/paper-styles/color.js';
import '../common/icons.js';
import '../common/styles.js';
import '../untrusted/untrusted_shared_vars_css.js';
// Import load_time_data.js first because strings.js requires
// window.load_time_data to be initialized.
import '/load_time_data.js';
import '/strings.js';
