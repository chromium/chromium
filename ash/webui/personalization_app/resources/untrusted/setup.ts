// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview imports javascript files that have side effects. Makes sure
 * they are imported in the correct order.
 */

// Import necessary built in modules before ../common files. This will
// guarantee that polymer and certain polymer elements are loaded first.
import '//resources/polymer/v3_0/iron-icon/iron-icon.js';
import '//resources/cr_elements/shared_vars_css.m.js';
import '../common/icons.html.js';
import '../common/common_style.css.js';
import '/strings.m.js';
