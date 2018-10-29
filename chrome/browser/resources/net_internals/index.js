// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// <include src="util.js">
// <include src="view.js">
// <include src="tab_switcher_view.js">
// <include src="domain_security_policy_view.js">
// <include src="browser_bridge.js">
// <include src="main.js">
// <include src="dns_view.js">
// <include src="events_view.js">
// <include src="proxy_view.js">
// <include src="sockets_view.js">
// <include src="chromeos_view.js">

document.addEventListener('DOMContentLoaded', function() {
  MainView.getInstance();  // from main.js
});
