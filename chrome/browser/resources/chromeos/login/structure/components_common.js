// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off

function loadCommonComponents() {
// TODO(crbug.com/1111387) - Remove excessive logging.
  console.warn('loadCommonComponents() : Starting to load common components.');
// This inclusion is types-only. No actual code to execute.

// <include src="../gaia_dialog.js">

// <include src="../screen_gaia_signin.js">
// <include src="../saml_confirm_password.js">
// <include src="../recommend_apps.js">
// <include src="../oobe_screen_assistant_optin_flow.js">
// <include src="../multidevice_setup_first_run.js">
// <include src="../screen_multidevice_setup.js">

// <include src="components_[OOBE].js">
// TODO(crbug.com/1111387) - Remove excessive logging.
  console.warn('loadCommonComponents() : Common components have loaded.');
}
